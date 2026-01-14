#include "quantumliquidity/risk/risk_manager.hpp"
#include "quantumliquidity/execution/position_manager.hpp"
#include "quantumliquidity/common/logger.hpp"
#include <cmath>
#include <algorithm>
#include <chrono>

namespace quantumliquidity::risk {

RiskManager::RiskManager(const RiskLimits& limits)
    : limits_(limits)
    , position_mgr_(nullptr)
    , daily_pnl_(0.0)
    , daily_high_pnl_(0.0)
    , daily_realized_pnl_(0.0)
    , orders_submitted_today_(0)
    , orders_filled_today_(0)
    , orders_rejected_today_(0)
    , orders_cancelled_today_(0)
    , halt_active_(false)
{
    Logger::info("risk", "Risk manager initialized");
    Logger::info("risk", "Limits: max_position={:.2f}, max_exposure={:.2f}, max_daily_loss={:.2f}",
        limits_.max_position_size, limits_.max_total_exposure, limits_.max_daily_loss);
}

RiskManager::~RiskManager() {
    Logger::info("risk", "Risk manager shutdown");
}

execution::RiskCheckResult RiskManager::check_order(
    const execution::OrderRequest& order,
    double current_price
) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    execution::RiskCheckResult result;
    result.allowed = false;
    result.reserved_capital = 0.0;
    result.new_exposure = 0.0;
    result.new_position_size = 0.0;
    
    // 1. Check if trading is halted
    if (halt_active_) {
        result.reason = "Trading halted: " + halt_reason_;
        orders_rejected_today_++;
        Logger::warning("risk", "Order rejected (halted): id={}, reason={}",
            order.order_id, result.reason);
        return result;
    }
    
    // 2. Validate order parameters
    if (order.quantity <= 0) {
        result.reason = "Invalid quantity: must be > 0";
        orders_rejected_today_++;
        Logger::warning("risk", "Order rejected: id={}, reason={}", order.order_id, result.reason);
        return result;
    }
    
    if (order.type == execution::OrderType::LIMIT && order.price <= 0) {
        result.reason = "Invalid limit price: must be > 0";
        orders_rejected_today_++;
        Logger::warning("risk", "Order rejected: id={}, reason={}", order.order_id, result.reason);
        return result;
    }
    
    // 3. Check order size limit
    if (order.quantity > limits_.max_order_size) {
        result.reason = "Order size exceeds limit: " + std::to_string(order.quantity) +
                       " > " + std::to_string(limits_.max_order_size);
        orders_rejected_today_++;
        Logger::warning("risk", "Order rejected: id={}, reason={}", order.order_id, result.reason);
        return result;
    }
    
    // 4. Check rate limiting
    if (!check_rate_limit()) {
        result.reason = "Order rate limit exceeded: " +
                       std::to_string(limits_.max_orders_per_minute) + " orders/min";
        orders_rejected_today_++;
        Logger::warning("risk", "Order rejected: id={}, reason={}", order.order_id, result.reason);
        return result;
    }
    
    // 5. Check daily order limit
    if (orders_submitted_today_ >= limits_.max_orders_per_day) {
        result.reason = "Daily order limit exceeded: " + std::to_string(limits_.max_orders_per_day);
        orders_rejected_today_++;
        Logger::warning("risk", "Order rejected: id={}, reason={}", order.order_id, result.reason);
        return result;
    }
    
    // 6. Calculate order value
    double order_price = order.type == execution::OrderType::MARKET ? current_price : order.price;
    double order_value = calculate_order_value(order, order_price);
    
    // 7. Get current position
    double current_qty = 0.0;
    if (position_mgr_) {
        current_qty = position_mgr_->get_quantity(order.instrument);
    }
    
    // 8. Calculate new position after this order
    double signed_order_qty = (order.side == execution::OrderSide::BUY ? 1.0 : -1.0) * order.quantity;
    double new_qty = current_qty + signed_order_qty;
    result.new_position_size = std::abs(new_qty);
    
    // 9. Check position size limit
    if (!check_position_size_limit(order.instrument, new_qty)) {
        result.reason = "Position size limit exceeded: new_qty=" + std::to_string(new_qty) +
                       ", limit=" + std::to_string(limits_.max_position_size);
        orders_rejected_today_++;
        Logger::warning("risk", "Order rejected: id={}, reason={}", order.order_id, result.reason);
        return result;
    }
    
    // 10. Check exposure limit
    double additional_exposure = std::abs(signed_order_qty * order_price);
    if (!check_exposure_limit(additional_exposure)) {
        result.reason = "Exposure limit exceeded: would add " +
                       std::to_string(additional_exposure) + ", limit=" +
                       std::to_string(limits_.max_total_exposure);
        orders_rejected_today_++;
        Logger::warning("risk", "Order rejected: id={}, reason={}", order.order_id, result.reason);
        return result;
    }
    
    // 11. Check daily loss limit
    if (!check_daily_loss_limit()) {
        result.reason = "Daily loss limit exceeded: " + std::to_string(daily_pnl_) +
                       ", limit=" + std::to_string(-limits_.max_daily_loss);
        halt_active_ = true;
        halt_reason_ = result.reason;
        orders_rejected_today_++;
        Logger::error("risk", "Order rejected and HALT: id={}, reason={}", order.order_id, result.reason);
        return result;
    }
    
    // 12. Check free capital requirement
    double total_reserved = 0.0;
    for (const auto& [oid, reserved] : reserved_by_order_) {
        total_reserved += reserved;
    }
    
    double total_exposure = 0.0;
    if (position_mgr_) {
        total_exposure = position_mgr_->get_total_exposure(market_prices_);
    }
    
    double used_capital = total_exposure + total_reserved + order_value;
    double free_capital = limits_.bankroll - used_capital;
    double min_free = limits_.bankroll * limits_.min_free_capital_pct;
    
    if (free_capital < min_free) {
        result.reason = "Insufficient free capital: " + std::to_string(free_capital) +
                       " < " + std::to_string(min_free);
        orders_rejected_today_++;
        Logger::warning("risk", "Order rejected: id={}, reason={}", order.order_id, result.reason);
        return result;
    }
    
    // All checks passed!
    result.allowed = true;
    result.reason = "OK";
    result.reserved_capital = order_value;
    result.new_exposure = total_exposure + order_value;
    
    // Reserve capital for this order
    reserved_by_order_[order.order_id] = order_value;
    orders_submitted_today_++;
    
    // Add to rate limit tracking
    auto now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    recent_order_timestamps_.push_back(now_ns);
    
    Logger::info("risk", "Order approved: id={}, instrument={}, qty={:.2f}, reserved={:.2f}",
        order.order_id, order.instrument, order.quantity, order_value);
    
    return result;
}

void RiskManager::on_fill(const execution::Fill& fill) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    orders_filled_today_++;
    
    // Free reserved capital for this order
    auto it = reserved_by_order_.find(fill.order_id);
    if (it != reserved_by_order_.end()) {
        Logger::debug("risk", "Freeing reserved capital for order {}: {:.2f}",
            fill.order_id, it->second);
        reserved_by_order_.erase(it);
    }
    
    // Update PnL
    update_pnl(fill);
    
    // Check if we hit new daily high
    if (daily_pnl_ > daily_high_pnl_) {
        daily_high_pnl_ = daily_pnl_;
    }
    
    // Check if we should halt due to drawdown
    double drawdown_from_high = daily_high_pnl_ - daily_pnl_;
    if (drawdown_from_high > limits_.max_drawdown_from_high) {
        halt_active_ = true;
        halt_reason_ = "Max drawdown from high exceeded: " + std::to_string(drawdown_from_high);
        Logger::error("risk", "HALT TRIGGERED: {}", halt_reason_);
    }
}

void RiskManager::on_order_rejected(const std::string& order_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    orders_rejected_today_++;
    
    // Free reserved capital
    auto it = reserved_by_order_.find(order_id);
    if (it != reserved_by_order_.end()) {
        Logger::debug("risk", "Freeing reserved capital for rejected order {}: {:.2f}",
            order_id, it->second);
        reserved_by_order_.erase(it);
    }
}

void RiskManager::on_order_cancelled(const std::string& order_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    orders_cancelled_today_++;
    
    // Free reserved capital
    auto it = reserved_by_order_.find(order_id);
    if (it != reserved_by_order_.end()) {
        Logger::debug("risk", "Freeing reserved capital for cancelled order {}: {:.2f}",
            order_id, it->second);
        reserved_by_order_.erase(it);
    }
}

bool RiskManager::should_halt() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return halt_active_;
}

std::string RiskManager::get_halt_reason() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return halt_active_ ? halt_reason_ : "";
}

execution::RiskMetrics RiskManager::get_metrics() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    execution::RiskMetrics metrics;
    
    // Get position metrics
    if (position_mgr_) {
        metrics.total_exposure = position_mgr_->get_total_exposure(market_prices_);
        metrics.realized_pnl = position_mgr_->get_total_realized_pnl();
        metrics.unrealized_pnl = position_mgr_->get_total_unrealized_pnl(market_prices_);
    } else {
        metrics.total_exposure = 0.0;
        metrics.realized_pnl = 0.0;
        metrics.unrealized_pnl = 0.0;
    }
    
    metrics.daily_pnl = daily_pnl_;
    metrics.daily_high_pnl = daily_high_pnl_;
    metrics.max_dd_today = daily_high_pnl_ - daily_pnl_;
    metrics.account_utilization = (metrics.total_exposure / limits_.bankroll) * 100.0;
    
    metrics.orders_submitted_today = orders_submitted_today_;
    metrics.orders_filled_today = orders_filled_today_;
    metrics.orders_rejected_today = orders_rejected_today_;
    metrics.orders_cancelled_today = orders_cancelled_today_;
    
    metrics.halt_active = halt_active_;
    metrics.halt_reason = halt_reason_;
    
    metrics.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    return metrics;
}

void RiskManager::reset_daily() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    daily_pnl_ = 0.0;
    daily_high_pnl_ = 0.0;
    daily_realized_pnl_ = 0.0;
    orders_submitted_today_ = 0;
    orders_filled_today_ = 0;
    orders_rejected_today_ = 0;
    orders_cancelled_today_ = 0;
    recent_order_timestamps_.clear();
    reserved_by_order_.clear();
    
    halt_active_ = false;
    halt_reason_.clear();
    
    Logger::info("risk", "Daily counters reset");
}

void RiskManager::set_position_manager(execution::PositionManager* position_mgr) {
    std::lock_guard<std::mutex> lock(mutex_);
    position_mgr_ = position_mgr;
}

void RiskManager::update_market_prices(const std::map<std::string, double>& prices) {
    std::lock_guard<std::mutex> lock(mutex_);
    market_prices_ = prices;
    
    // Recalculate daily PnL
    if (position_mgr_) {
        daily_pnl_ = position_mgr_->get_total_realized_pnl() +
                    position_mgr_->get_total_unrealized_pnl(market_prices_);
    }
}

// Private helper methods

double RiskManager::calculate_order_value(const execution::OrderRequest& order, double price) const {
    return std::abs(order.quantity * price);
}

bool RiskManager::check_position_size_limit(const std::string& instrument, double new_qty) const {
    return std::abs(new_qty) <= limits_.max_position_size;
}

bool RiskManager::check_exposure_limit(double additional_exposure) const {
    double current_exposure = 0.0;
    if (position_mgr_) {
        current_exposure = position_mgr_->get_total_exposure(market_prices_);
    }
    
    double total_reserved = 0.0;
    for (const auto& [oid, reserved] : reserved_by_order_) {
        total_reserved += reserved;
    }
    
    double new_total_exposure = current_exposure + total_reserved + additional_exposure;
    return new_total_exposure <= limits_.max_total_exposure;
}

bool RiskManager::check_daily_loss_limit() const {
    return daily_pnl_ >= -limits_.max_daily_loss;
}

bool RiskManager::check_rate_limit() {
    // Remove timestamps older than 1 minute
    auto now_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    
    int64_t one_minute_ns = 60LL * 1000000000LL;
    
    recent_order_timestamps_.erase(
        std::remove_if(recent_order_timestamps_.begin(), recent_order_timestamps_.end(),
            [now_ns, one_minute_ns](int64_t ts) { return (now_ns - ts) > one_minute_ns; }),
        recent_order_timestamps_.end()
    );
    
    return recent_order_timestamps_.size() < static_cast<size_t>(limits_.max_orders_per_minute);
}

void RiskManager::update_pnl(const execution::Fill& fill) {
    // PnL calculation is handled by PositionManager
    // Here we just update our tracking
    if (position_mgr_) {
        daily_pnl_ = position_mgr_->get_total_realized_pnl() +
                    position_mgr_->get_total_unrealized_pnl(market_prices_);
    }
}

} // namespace quantumliquidity::risk
