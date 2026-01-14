#include "quantumliquidity/execution/execution_engine.hpp"
#include "quantumliquidity/common/logger.hpp"
#include <chrono>
#include <algorithm>

namespace quantumliquidity::execution {

ExecutionEngine::ExecutionEngine(
    const Config& config,
    risk::RiskManager* risk_mgr,
    PositionManager* position_mgr
)
    : config_(config)
    , risk_mgr_(risk_mgr)
    , position_mgr_(position_mgr)
    , shutdown_requested_(false)
{
    if (!risk_mgr_) {
        throw std::invalid_argument("Risk manager cannot be null");
    }
    if (!position_mgr_) {
        throw std::invalid_argument("Position manager cannot be null");
    }

    // Initialize stats
    stats_.total_orders_submitted = 0;
    stats_.total_orders_filled = 0;
    stats_.total_orders_rejected = 0;
    stats_.total_orders_cancelled = 0;
    stats_.active_orders = 0;
    stats_.total_volume_traded = 0.0;
    stats_.last_fill_timestamp_ns = 0;

    // Connect to Redis if enabled
    if (config_.enable_redis) {
        try {
            redis_ = std::make_unique<common::RedisClient>(
                config_.redis_host,
                config_.redis_port,
                config_.redis_password
            );
            redis_->connect();
            Logger::info("execution", "Redis connected: {}:{}",
                config_.redis_host, config_.redis_port);
        } catch (const std::exception& e) {
            Logger::error("execution", "Redis connection failed: {}", e.what());
            redis_.reset();
        }
    }

    Logger::info("execution", "Execution engine initialized");
}

ExecutionEngine::~ExecutionEngine() {
    shutdown();
    Logger::info("execution", "Execution engine shutdown");
}

void ExecutionEngine::register_provider(
    const std::string& name,
    std::shared_ptr<IExecutionProvider> provider
) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!provider) {
        Logger::error("execution", "Cannot register null provider: {}", name);
        return;
    }

    provider->set_execution_engine(this);
    providers_[name] = provider;

    // Set as default if first provider
    if (default_provider_.empty()) {
        default_provider_ = name;
    }

    Logger::info("execution", "Registered execution provider: {}", name);
}

void ExecutionEngine::set_instrument_provider(
    const std::string& instrument,
    const std::string& provider_name
) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (providers_.find(provider_name) == providers_.end()) {
        Logger::error("execution", "Unknown provider: {}", provider_name);
        return;
    }

    instrument_routing_[instrument] = provider_name;
    Logger::info("execution", "Routing {} -> {}", instrument, provider_name);
}

OrderUpdate ExecutionEngine::submit_order(const OrderRequest& order) {
    std::lock_guard<std::mutex> lock(mutex_);

    OrderUpdate result;
    result.order_id = order.order_id;
    result.status = OrderStatus::REJECTED;
    result.filled_qty = 0.0;
    result.remaining_qty = order.quantity;
    result.avg_fill_price = 0.0;
    result.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    if (shutdown_requested_) {
        result.reason = "Execution engine shutting down";
        stats_.total_orders_rejected++;
        Logger::warning("execution", "Order rejected (shutdown): id={}", order.order_id);
        publish_order_event(result);
        return result;
    }

    // 1. Risk check
    Logger::debug("execution", "Checking order: id={}, instrument={}, side={}, qty={:.2f}",
        order.order_id, order.instrument, order_side_to_string(order.side), order.quantity);

    // Get current price for risk check (simplified - in production get from market data)
    double current_price = order.type == OrderType::MARKET ? 0.0 : order.price;
    if (current_price == 0.0) {
        current_price = 1.0;  // Fallback - real implementation needs market data feed
    }

    auto risk_result = risk_mgr_->check_order(order, current_price);

    if (!risk_result.allowed) {
        result.reason = "Risk check failed: " + risk_result.reason;
        stats_.total_orders_rejected++;
        risk_mgr_->on_order_rejected(order.order_id);
        Logger::warning("execution", "Order rejected (risk): id={}, reason={}",
            order.order_id, result.reason);
        publish_order_event(result);
        return result;
    }

    // 2. Select provider
    std::string provider_name = select_provider_for_order(order);
    if (provider_name.empty() || providers_.find(provider_name) == providers_.end()) {
        result.reason = "No execution provider available for " + order.instrument;
        stats_.total_orders_rejected++;
        risk_mgr_->on_order_rejected(order.order_id);
        Logger::error("execution", "Order rejected (no provider): id={}, instrument={}",
            order.order_id, order.instrument);
        publish_order_event(result);
        return result;
    }

    auto provider = providers_[provider_name];

    // 3. Check provider connection
    if (!provider->is_connected()) {
        result.reason = "Provider not connected: " + provider_name;
        stats_.total_orders_rejected++;
        risk_mgr_->on_order_rejected(order.order_id);
        Logger::error("execution", "Order rejected (disconnected): id={}, provider={}",
            order.order_id, provider_name);
        publish_order_event(result);
        return result;
    }

    // 4. Submit to provider
    Logger::info("execution", "Submitting order: id={}, instrument={}, qty={:.2f} via {}",
        order.order_id, order.instrument, order.quantity, provider_name);

    try {
        result = provider->submit_order(order);

        // 5. Track order state
        if (result.status != OrderStatus::REJECTED) {
            OrderState state;
            state.request = order;
            state.current_status = result;
            state.provider_name = provider_name;
            state.submit_timestamp_ns = result.timestamp_ns;
            state.last_update_ns = result.timestamp_ns;

            active_orders_[order.order_id] = state;
            stats_.total_orders_submitted++;
            stats_.active_orders++;

            Logger::info("execution", "Order submitted: id={}, status={}",
                order.order_id, order_status_to_string(result.status));
        } else {
            stats_.total_orders_rejected++;
            risk_mgr_->on_order_rejected(order.order_id);
            Logger::warning("execution", "Order rejected by provider: id={}, reason={}",
                order.order_id, result.reason);
        }

    } catch (const std::exception& e) {
        result.status = OrderStatus::ERROR;
        result.reason = "Provider exception: " + std::string(e.what());
        stats_.total_orders_rejected++;
        risk_mgr_->on_order_rejected(order.order_id);
        Logger::error("execution", "Order submission failed: id={}, error={}",
            order.order_id, e.what());
    }

    // 6. Publish event
    publish_order_event(result);

    // Notify callbacks
    for (auto& callback : order_callbacks_) {
        try {
            callback(result);
        } catch (const std::exception& e) {
            Logger::error("execution", "Order callback exception: {}", e.what());
        }
    }

    return result;
}

OrderUpdate ExecutionEngine::cancel_order(const std::string& order_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    OrderUpdate result;
    result.order_id = order_id;
    result.status = OrderStatus::REJECTED;
    result.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    auto it = active_orders_.find(order_id);
    if (it == active_orders_.end()) {
        result.reason = "Order not found or already completed";
        Logger::warning("execution", "Cancel failed: order {} not found", order_id);
        return result;
    }

    const OrderState& state = it->second;
    auto provider = providers_[state.provider_name];

    if (!provider) {
        result.reason = "Provider not available: " + state.provider_name;
        Logger::error("execution", "Cancel failed: provider {} not available",
            state.provider_name);
        return result;
    }

    Logger::info("execution", "Cancelling order: id={}", order_id);

    try {
        result = provider->cancel_order(order_id);

        if (result.status == OrderStatus::CANCELLED) {
            stats_.total_orders_cancelled++;
            risk_mgr_->on_order_cancelled(order_id);
            finalize_order(order_id);
            Logger::info("execution", "Order cancelled: id={}", order_id);
        }

    } catch (const std::exception& e) {
        result.status = OrderStatus::ERROR;
        result.reason = "Cancel exception: " + std::string(e.what());
        Logger::error("execution", "Cancel failed: id={}, error={}", order_id, e.what());
    }

    publish_order_event(result);

    // Notify callbacks
    for (auto& callback : order_callbacks_) {
        try {
            callback(result);
        } catch (const std::exception& e) {
            Logger::error("execution", "Order callback exception: {}", e.what());
        }
    }

    return result;
}

OrderUpdate ExecutionEngine::modify_order(const OrderModification& modification) {
    std::lock_guard<std::mutex> lock(mutex_);

    OrderUpdate result;
    result.order_id = modification.order_id;
    result.status = OrderStatus::REJECTED;
    result.timestamp_ns = modification.timestamp_ns;

    auto it = active_orders_.find(modification.order_id);
    if (it == active_orders_.end()) {
        result.reason = "Order not found or already completed";
        Logger::warning("execution", "Modify failed: order {} not found",
            modification.order_id);
        return result;
    }

    const OrderState& state = it->second;
    auto provider = providers_[state.provider_name];

    if (!provider) {
        result.reason = "Provider not available: " + state.provider_name;
        Logger::error("execution", "Modify failed: provider {} not available",
            state.provider_name);
        return result;
    }

    Logger::info("execution", "Modifying order: id={}", modification.order_id);

    try {
        result = provider->modify_order(modification);
        Logger::info("execution", "Order modified: id={}, status={}",
            modification.order_id, order_status_to_string(result.status));

    } catch (const std::exception& e) {
        result.status = OrderStatus::ERROR;
        result.reason = "Modify exception: " + std::string(e.what());
        Logger::error("execution", "Modify failed: id={}, error={}",
            modification.order_id, e.what());
    }

    publish_order_event(result);

    return result;
}

void ExecutionEngine::on_fill(const Fill& fill) {
    std::lock_guard<std::mutex> lock(mutex_);

    Logger::info("execution", "Fill received: id={}, order={}, instrument={}, "
        "side={}, qty={:.2f}, price={:.5f}",
        fill.fill_id, fill.order_id, fill.instrument,
        order_side_to_string(fill.side), fill.quantity, fill.price);

    // Update position manager
    position_mgr_->on_fill(fill);

    // Notify risk manager
    risk_mgr_->on_fill(fill);

    // Update stats
    stats_.total_orders_filled++;
    stats_.total_volume_traded += fill.quantity;
    stats_.last_fill_timestamp_ns = fill.timestamp_ns;

    // Update order state
    auto it = active_orders_.find(fill.order_id);
    if (it != active_orders_.end()) {
        OrderState& state = it->second;
        state.current_status.filled_qty += fill.quantity;
        state.current_status.remaining_qty =
            state.request.quantity - state.current_status.filled_qty;

        // Update average fill price
        double total_value = state.current_status.avg_fill_price *
            (state.current_status.filled_qty - fill.quantity) +
            fill.price * fill.quantity;
        state.current_status.avg_fill_price = total_value / state.current_status.filled_qty;

        state.last_update_ns = fill.timestamp_ns;

        // Check if order fully filled
        if (state.current_status.remaining_qty <= 1e-8) {
            state.current_status.status = OrderStatus::FILLED;
            finalize_order(fill.order_id);
            Logger::info("execution", "Order fully filled: id={}", fill.order_id);
        } else {
            state.current_status.status = OrderStatus::PARTIALLY_FILLED;
        }
    }

    // Publish fill event
    publish_fill_event(fill);

    // Notify callbacks
    for (auto& callback : fill_callbacks_) {
        try {
            callback(fill);
        } catch (const std::exception& e) {
            Logger::error("execution", "Fill callback exception: {}", e.what());
        }
    }
}

void ExecutionEngine::on_order_update(const OrderUpdate& update) {
    std::lock_guard<std::mutex> lock(mutex_);

    Logger::debug("execution", "Order update: id={}, status={}",
        update.order_id, order_status_to_string(update.status));

    update_order_state(update);
    publish_order_event(update);

    // Notify callbacks
    for (auto& callback : order_callbacks_) {
        try {
            callback(update);
        } catch (const std::exception& e) {
            Logger::error("execution", "Order callback exception: {}", e.what());
        }
    }
}

std::optional<OrderUpdate> ExecutionEngine::get_order_status(
    const std::string& order_id
) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = active_orders_.find(order_id);
    if (it != active_orders_.end()) {
        return it->second.current_status;
    }

    auto completed_it = completed_orders_.find(order_id);
    if (completed_it != completed_orders_.end()) {
        return completed_it->second.current_status;
    }

    return std::nullopt;
}

std::map<std::string, OrderUpdate> ExecutionEngine::get_active_orders() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::map<std::string, OrderUpdate> result;
    for (const auto& [order_id, state] : active_orders_) {
        result[order_id] = state.current_status;
    }
    return result;
}

void ExecutionEngine::register_order_callback(OrderEventCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    order_callbacks_.push_back(callback);
}

void ExecutionEngine::register_fill_callback(FillEventCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    fill_callbacks_.push_back(callback);
}

ExecutionEngine::Stats ExecutionEngine::get_stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

void ExecutionEngine::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (shutdown_requested_) {
        return;
    }

    shutdown_requested_ = true;
    Logger::info("execution", "Shutting down execution engine...");

    // Cancel all active orders
    std::vector<std::string> order_ids;
    for (const auto& [order_id, _] : active_orders_) {
        order_ids.push_back(order_id);
    }

    for (const auto& order_id : order_ids) {
        try {
            cancel_order(order_id);
        } catch (const std::exception& e) {
            Logger::error("execution", "Error cancelling order {} during shutdown: {}",
                order_id, e.what());
        }
    }

    // Disconnect providers
    for (auto& [name, provider] : providers_) {
        try {
            provider->disconnect();
            Logger::info("execution", "Disconnected provider: {}", name);
        } catch (const std::exception& e) {
            Logger::error("execution", "Error disconnecting provider {}: {}",
                name, e.what());
        }
    }

    // Disconnect Redis
    if (redis_) {
        redis_->disconnect();
        redis_.reset();
    }

    Logger::info("execution", "Execution engine shutdown complete");
}

// Private helper methods

std::string ExecutionEngine::select_provider_for_order(
    const OrderRequest& order
) const {
    // Check instrument-specific routing
    auto it = instrument_routing_.find(order.instrument);
    if (it != instrument_routing_.end()) {
        return it->second;
    }

    // Use default provider
    return default_provider_;
}

void ExecutionEngine::publish_order_event(const OrderUpdate& update) {
    if (!redis_ || !config_.enable_redis) {
        return;
    }

    try {
        // Format: channel = "orders", message = JSON
        std::string json = "{"
            "\"order_id\":\"" + update.order_id + "\","
            "\"status\":\"" + std::string(order_status_to_string(update.status)) + "\","
            "\"filled_qty\":" + std::to_string(update.filled_qty) + ","
            "\"remaining_qty\":" + std::to_string(update.remaining_qty) + ","
            "\"avg_fill_price\":" + std::to_string(update.avg_fill_price) + ","
            "\"reason\":\"" + update.reason + "\","
            "\"timestamp_ns\":" + std::to_string(update.timestamp_ns) +
        "}";

        redis_->publish("orders", json);
    } catch (const std::exception& e) {
        Logger::error("execution", "Failed to publish order event: {}", e.what());
    }
}

void ExecutionEngine::publish_fill_event(const Fill& fill) {
    if (!redis_ || !config_.enable_redis) {
        return;
    }

    try {
        // Format: channel = "fills", message = JSON
        std::string json = "{"
            "\"fill_id\":\"" + fill.fill_id + "\","
            "\"order_id\":\"" + fill.order_id + "\","
            "\"instrument\":\"" + fill.instrument + "\","
            "\"side\":\"" + std::string(order_side_to_string(fill.side)) + "\","
            "\"quantity\":" + std::to_string(fill.quantity) + ","
            "\"price\":" + std::to_string(fill.price) + ","
            "\"commission\":" + std::to_string(fill.commission) + ","
            "\"timestamp_ns\":" + std::to_string(fill.timestamp_ns) +
        "}";

        redis_->publish("fills", json);
    } catch (const std::exception& e) {
        Logger::error("execution", "Failed to publish fill event: {}", e.what());
    }
}

void ExecutionEngine::update_order_state(const OrderUpdate& update) {
    auto it = active_orders_.find(update.order_id);
    if (it == active_orders_.end()) {
        return;
    }

    OrderState& state = it->second;
    state.current_status = update;
    state.last_update_ns = update.timestamp_ns;

    // Check if order is terminal state
    if (update.status == OrderStatus::FILLED ||
        update.status == OrderStatus::CANCELLED ||
        update.status == OrderStatus::REJECTED ||
        update.status == OrderStatus::ERROR ||
        update.status == OrderStatus::EXPIRED) {
        finalize_order(update.order_id);
    }
}

void ExecutionEngine::finalize_order(const std::string& order_id) {
    auto it = active_orders_.find(order_id);
    if (it == active_orders_.end()) {
        return;
    }

    // Move to completed orders (keep recent history)
    completed_orders_[order_id] = it->second;
    active_orders_.erase(it);
    stats_.active_orders--;

    // Limit completed orders history
    if (completed_orders_.size() > 1000) {
        // Remove oldest
        auto oldest = completed_orders_.begin();
        completed_orders_.erase(oldest);
    }

    Logger::debug("execution", "Order finalized: id={}", order_id);
}

} // namespace quantumliquidity::execution
