#include "quantumliquidity/execution/providers/mock_broker.hpp"
#include "quantumliquidity/common/logger.hpp"
#include <chrono>
#include <sstream>
#include <iomanip>

namespace quantumliquidity::execution {

MockBroker::MockBroker(const Config& config)
    : config_(config)
    , engine_(nullptr)
    , connected_(false)
    , shutdown_requested_(false)
    , next_fill_id_(1)
    , rng_(std::random_device{}())
    , uniform_dist_(0.0, 1.0)
{
    if (config_.auto_connect) {
        connect();
    }

    Logger::info("execution", "Mock broker initialized: {}", config_.broker_name);
}

MockBroker::~MockBroker() {
    disconnect();
}

bool MockBroker::connect() {
    std::lock_guard<std::mutex> lock(mutex_);

    if (connected_) {
        return true;
    }

    connected_ = true;
    Logger::info("execution", "Mock broker connected: {}", config_.broker_name);
    return true;
}

void MockBroker::disconnect() {
    {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!connected_) {
            return;
        }

        shutdown_requested_ = true;
        connected_ = false;
    }

    // Wait for fill threads to complete
    for (auto& thread : fill_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    fill_threads_.clear();

    Logger::info("execution", "Mock broker disconnected: {}", config_.broker_name);
}

bool MockBroker::is_connected() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return connected_;
}

std::string MockBroker::get_name() const {
    return config_.broker_name;
}

void MockBroker::set_execution_engine(ExecutionEngine* engine) {
    std::lock_guard<std::mutex> lock(mutex_);
    engine_ = engine;
}

void MockBroker::set_market_price(const std::string& instrument, double price) {
    std::lock_guard<std::mutex> lock(mutex_);
    market_prices_[instrument] = price;
    Logger::debug("execution", "Mock broker: set market price {}={:.5f}",
        instrument, price);
}

OrderUpdate MockBroker::submit_order(const OrderRequest& order) {
    std::lock_guard<std::mutex> lock(mutex_);

    stats_.orders_received++;

    OrderUpdate result;
    result.order_id = order.order_id;
    result.filled_qty = 0.0;
    result.remaining_qty = order.quantity;
    result.avg_fill_price = 0.0;
    result.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    // Check if should reject
    if (should_reject()) {
        result.status = OrderStatus::REJECTED;
        result.reason = "Random rejection (simulated)";
        stats_.orders_rejected++;
        Logger::warning("execution", "Mock broker rejected order: {}", order.order_id);
        return result;
    }

    // Validate basic parameters
    if (order.quantity <= 0) {
        result.status = OrderStatus::REJECTED;
        result.reason = "Invalid quantity";
        stats_.orders_rejected++;
        return result;
    }

    if (order.type == OrderType::LIMIT && order.price <= 0) {
        result.status = OrderStatus::REJECTED;
        result.reason = "Invalid limit price";
        stats_.orders_rejected++;
        return result;
    }

    // Accept order
    result.status = OrderStatus::ACKNOWLEDGED;
    result.reason = "Order accepted by mock broker";

    // Store order state
    OrderState state;
    state.request = order;
    state.current_status = result;
    state.filled_qty = 0.0;
    state.remaining_qty = order.quantity;
    state.submit_timestamp_ns = result.timestamp_ns;
    state.cancelled = false;

    orders_[order.order_id] = state;

    Logger::info("execution", "Mock broker accepted order: id={}, instrument={}, "
        "side={}, qty={:.2f}",
        order.order_id, order.instrument,
        order_side_to_string(order.side), order.quantity);

    // Simulate fill asynchronously
    if (!shutdown_requested_) {
        std::thread fill_thread([this, order_id = order.order_id]() {
            simulate_fill(order_id);
        });
        fill_threads_.push_back(std::move(fill_thread));
    }

    return result;
}

OrderUpdate MockBroker::cancel_order(const std::string& order_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    OrderUpdate result;
    result.order_id = order_id;
    result.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    auto it = orders_.find(order_id);
    if (it == orders_.end()) {
        result.status = OrderStatus::REJECTED;
        result.reason = "Order not found";
        return result;
    }

    OrderState& state = it->second;

    // Check if already terminal state
    if (state.current_status.status == OrderStatus::FILLED ||
        state.current_status.status == OrderStatus::CANCELLED ||
        state.current_status.status == OrderStatus::REJECTED) {
        result.status = state.current_status.status;
        result.reason = "Order already in terminal state";
        result.filled_qty = state.current_status.filled_qty;
        result.remaining_qty = state.current_status.remaining_qty;
        result.avg_fill_price = state.current_status.avg_fill_price;
        return result;
    }

    // Cancel the order
    state.cancelled = true;
    state.current_status.status = OrderStatus::CANCELLED;
    state.current_status.reason = "Cancelled by user";

    result.status = OrderStatus::CANCELLED;
    result.reason = "Order cancelled";
    result.filled_qty = state.filled_qty;
    result.remaining_qty = state.remaining_qty;
    result.avg_fill_price = state.current_status.avg_fill_price;

    stats_.orders_cancelled++;

    Logger::info("execution", "Mock broker cancelled order: {}", order_id);

    return result;
}

OrderUpdate MockBroker::modify_order(const OrderModification& modification) {
    std::lock_guard<std::mutex> lock(mutex_);

    OrderUpdate result;
    result.order_id = modification.order_id;
    result.timestamp_ns = modification.timestamp_ns;

    auto it = orders_.find(modification.order_id);
    if (it == orders_.end()) {
        result.status = OrderStatus::REJECTED;
        result.reason = "Order not found";
        return result;
    }

    OrderState& state = it->second;

    // Apply modifications
    if (modification.new_price) {
        state.request.price = *modification.new_price;
        Logger::info("execution", "Mock broker modified order {} price: {:.5f}",
            modification.order_id, *modification.new_price);
    }

    if (modification.new_quantity) {
        double old_qty = state.request.quantity;
        state.request.quantity = *modification.new_quantity;
        state.remaining_qty = *modification.new_quantity - state.filled_qty;
        Logger::info("execution", "Mock broker modified order {} quantity: {:.2f} -> {:.2f}",
            modification.order_id, old_qty, *modification.new_quantity);
    }

    result.status = OrderStatus::ACKNOWLEDGED;
    result.reason = "Modification accepted";
    result.filled_qty = state.filled_qty;
    result.remaining_qty = state.remaining_qty;
    result.avg_fill_price = state.current_status.avg_fill_price;

    state.current_status = result;

    return result;
}

std::optional<OrderUpdate> MockBroker::get_order_status(const std::string& order_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = orders_.find(order_id);
    if (it != orders_.end()) {
        return it->second.current_status;
    }

    return std::nullopt;
}

MockBroker::Stats MockBroker::get_stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return stats_;
}

// Private helper methods

void MockBroker::simulate_fill(const std::string& order_id) {
    // Sleep to simulate fill latency
    std::this_thread::sleep_for(std::chrono::milliseconds(config_.fill_latency_ms));

    std::lock_guard<std::mutex> lock(mutex_);

    if (shutdown_requested_) {
        return;
    }

    auto it = orders_.find(order_id);
    if (it == orders_.end()) {
        Logger::error("execution", "Mock broker: order {} not found for fill simulation",
            order_id);
        return;
    }

    OrderState& state = it->second;

    // Check if cancelled
    if (state.cancelled) {
        Logger::debug("execution", "Mock broker: order {} cancelled, no fill", order_id);
        return;
    }

    // Get market price
    double market_price = 0.0;
    auto price_it = market_prices_.find(state.request.instrument);
    if (price_it != market_prices_.end()) {
        market_price = price_it->second;
    } else {
        // Use order price as fallback
        market_price = state.request.type == OrderType::MARKET ? 100.0 : state.request.price;
    }

    // Determine number of fills
    int num_fills = config_.enable_partial_fills ? config_.partial_fill_count : 1;
    double qty_per_fill = state.remaining_qty / num_fills;

    // Generate fills
    for (int i = 0; i < num_fills && state.remaining_qty > 1e-8 && !state.cancelled; ++i) {
        double fill_qty = (i == num_fills - 1) ? state.remaining_qty : qty_per_fill;

        Fill fill;
        fill.fill_id = generate_fill_id();
        fill.order_id = order_id;
        fill.instrument = state.request.instrument;
        fill.side = state.request.side;
        fill.quantity = fill_qty;
        fill.price = calculate_fill_price(state.request, market_price);
        fill.commission = fill_qty * 0.0001;  // 1bp commission
        fill.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();

        // Update order state
        state.filled_qty += fill_qty;
        state.remaining_qty -= fill_qty;

        // Update average fill price
        double total_value = state.current_status.avg_fill_price *
            (state.filled_qty - fill_qty) + fill.price * fill_qty;
        state.current_status.avg_fill_price = total_value / state.filled_qty;
        state.current_status.filled_qty = state.filled_qty;
        state.current_status.remaining_qty = state.remaining_qty;

        // Update status
        if (state.remaining_qty <= 1e-8) {
            state.current_status.status = OrderStatus::FILLED;
            stats_.orders_filled++;
        } else {
            state.current_status.status = OrderStatus::PARTIALLY_FILLED;
        }

        stats_.fills_generated++;

        Logger::info("execution", "Mock broker generated fill: order={}, qty={:.2f}, "
            "price={:.5f}, remaining={:.2f}",
            order_id, fill_qty, fill.price, state.remaining_qty);

        // Notify execution engine
        if (engine_) {
            try {
                engine_->on_fill(fill);
            } catch (const std::exception& e) {
                Logger::error("execution", "Mock broker: error notifying engine of fill: {}",
                    e.what());
            }
        }

        // Sleep between partial fills
        if (i < num_fills - 1 && config_.enable_partial_fills) {
            std::this_thread::sleep_for(
                std::chrono::milliseconds(config_.fill_latency_ms / num_fills)
            );
        }
    }
}

std::string MockBroker::generate_fill_id() {
    std::ostringstream oss;
    oss << "FILL_" << config_.broker_name << "_"
        << std::setfill('0') << std::setw(8) << next_fill_id_++;
    return oss.str();
}

double MockBroker::calculate_fill_price(
    const OrderRequest& order,
    double market_price
) {
    double base_price = order.type == OrderType::MARKET ? market_price : order.price;

    // Apply slippage
    if (config_.slippage_bps > 0.0) {
        double slippage_factor = config_.slippage_bps / 10000.0;

        // Buy orders: slippage increases price
        // Sell orders: slippage decreases price
        if (order.side == OrderSide::BUY) {
            base_price *= (1.0 + slippage_factor);
        } else {
            base_price *= (1.0 - slippage_factor);
        }
    }

    return base_price;
}

bool MockBroker::should_reject() {
    if (config_.rejection_rate <= 0.0) {
        return false;
    }
    if (config_.rejection_rate >= 1.0) {
        return true;
    }

    return uniform_dist_(rng_) < config_.rejection_rate;
}

} // namespace quantumliquidity::execution
