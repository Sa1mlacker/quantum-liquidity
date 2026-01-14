#pragma once

#include "quantumliquidity/execution/types.hpp"
#include <map>
#include <mutex>
#include <memory>

namespace quantumliquidity::execution {
    class PositionManager;  // Forward declaration
}

namespace quantumliquidity::risk {

/**
 * @brief Risk configuration limits
 */
struct RiskLimits {
    // Position limits
    double max_position_size;               // Max contracts per instrument
    double max_total_exposure;              // Max $ exposure across all positions
    double max_single_position_pct;         // Max % of bankroll per position
    
    // Loss limits
    double max_daily_loss;                  // Max $ loss per day
    double max_daily_loss_pct;              // Max % loss per day
    double max_drawdown_from_high;          // Max drawdown from daily high
    
    // Order limits
    int max_orders_per_minute;              // Anti-fat-finger
    int max_orders_per_day;                 // Daily order limit
    double max_order_size;                  // Max size for single order
    
    // Account limits
    double bankroll;                        // Total capital
    double min_free_capital_pct;            // Min % of bankroll to keep free
};

/**
 * @brief Risk Manager - enforces trading limits
 *
 * Thread-safe pre-trade risk checks to prevent:
 * - Excessive position sizes
 * - Account overexposure
 * - Breaching daily loss limits
 * - Fat-finger trades
 */
class RiskManager {
public:
    explicit RiskManager(const RiskLimits& limits);
    ~RiskManager();

    /**
     * @brief Pre-trade risk check
     *
     * Validates order against all risk limits before submission.
     * Returns whether order is allowed and reason if rejected.
     *
     * @param order Order to check
     * @param current_price Current market price for the instrument
     * @return Risk check result with allow/reject decision
     */
    execution::RiskCheckResult check_order(
        const execution::OrderRequest& order,
        double current_price
    );

    /**
     * @brief Notify risk manager of fill
     *
     * Updates position tracking and PnL calculations.
     *
     * @param fill Executed fill
     */
    void on_fill(const execution::Fill& fill);

    /**
     * @brief Notify risk manager of order rejection
     *
     * Frees up reserved capital for rejected order.
     *
     * @param order_id Order that was rejected
     */
    void on_order_rejected(const std::string& order_id);

    /**
     * @brief Notify risk manager of order cancellation
     *
     * Frees up reserved capital for cancelled order.
     *
     * @param order_id Order that was cancelled
     */
    void on_order_cancelled(const std::string& order_id);

    /**
     * @brief Check if trading should be halted
     *
     * Returns true if daily loss limits exceeded or other halt conditions.
     *
     * @return true if trading should stop
     */
    bool should_halt() const;

    /**
     * @brief Get current halt reason
     *
     * @return Reason for halt, or empty string if not halted
     */
    std::string get_halt_reason() const;

    /**
     * @brief Get current risk metrics snapshot
     *
     * @return Current risk metrics
     */
    execution::RiskMetrics get_metrics() const;

    /**
     * @brief Reset daily counters
     *
     * Call at start of trading day (e.g., 00:00 UTC).
     * Resets PnL, order counts, but preserves positions.
     */
    void reset_daily();

    /**
     * @brief Set position manager
     *
     * Risk manager needs position manager to check current positions.
     *
     * @param position_mgr Pointer to position manager
     */
    void set_position_manager(execution::PositionManager* position_mgr);

    /**
     * @brief Update market prices
     *
     * Used for mark-to-market PnL calculation.
     *
     * @param prices Map of instrument to current price
     */
    void update_market_prices(const std::map<std::string, double>& prices);

private:
    // Helper: calculate order's capital requirement
    double calculate_order_value(const execution::OrderRequest& order, double price) const;
    
    // Helper: check position size limit
    bool check_position_size_limit(const std::string& instrument, double new_qty) const;
    
    // Helper: check exposure limit
    bool check_exposure_limit(double additional_exposure) const;
    
    // Helper: check daily loss limit
    bool check_daily_loss_limit() const;
    
    // Helper: check order rate limit
    bool check_rate_limit();
    
    // Helper: update PnL from fills
    void update_pnl(const execution::Fill& fill);

    RiskLimits limits_;
    execution::PositionManager* position_mgr_;
    
    // Daily tracking
    double daily_pnl_;
    double daily_high_pnl_;
    double daily_realized_pnl_;
    int orders_submitted_today_;
    int orders_filled_today_;
    int orders_rejected_today_;
    int orders_cancelled_today_;
    
    // Order rate limiting
    std::vector<int64_t> recent_order_timestamps_;  // Last N order timestamps
    
    // Reserved capital (for pending orders)
    std::map<std::string, double> reserved_by_order_;
    
    // Current market prices
    std::map<std::string, double> market_prices_;
    
    // Halt state
    bool halt_active_;
    std::string halt_reason_;
    
    mutable std::mutex mutex_;
};

} // namespace quantumliquidity::risk
