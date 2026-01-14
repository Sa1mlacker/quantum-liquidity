#pragma once

#include "quantumliquidity/common/types.hpp"
#include <map>
#include <memory>

namespace quantumliquidity::risk {

/**
 * @brief Risk limits configuration
 */
struct RiskLimits {
    // Position limits
    double max_position_value_per_instrument;  // USD
    double max_total_exposure;                 // USD
    std::map<InstrumentID, double> per_instrument_limits;  // Custom limits

    // Loss limits
    double max_daily_loss;                     // USD
    double max_drawdown_from_peak;             // USD

    // Order rate limits
    uint32_t max_orders_per_minute;
    uint32_t max_orders_per_hour;

    // Leverage
    double max_leverage;

    // Emergency kill-switch flag
    bool kill_switch_active = false;
};

/**
 * @brief Risk engine interface
 *
 * Pre-trade risk checks, position tracking, PnL calculation.
 */
class IRiskEngine {
public:
    virtual ~IRiskEngine() = default;

    // Configuration
    virtual void set_limits(const RiskLimits& limits) = 0;
    virtual RiskLimits get_limits() const = 0;

    // Pre-trade checks
    virtual RiskCheckResult check_order(const OrderRequest& order) = 0;

    // Position updates (called by execution engine on fills)
    virtual void update_position(const Fill& fill) = 0;
    virtual void update_market_price(const InstrumentID& instrument, Price price) = 0;

    // Position queries
    virtual Position get_position(const InstrumentID& instrument) const = 0;
    virtual std::vector<Position> get_all_positions() const = 0;

    // Metrics
    virtual RiskMetrics get_metrics() const = 0;

    // Emergency controls
    virtual void activate_kill_switch(const std::string& reason) = 0;
    virtual void deactivate_kill_switch() = 0;
    virtual bool is_kill_switch_active() const = 0;

    // Daily reset (call at start of trading day)
    virtual void reset_daily_counters() = 0;
};

/**
 * @brief Position manager (internal to risk engine)
 */
class IPositionManager {
public:
    virtual ~IPositionManager() = default;

    virtual void apply_fill(const Fill& fill) = 0;
    virtual Position get_position(const InstrumentID& instrument) const = 0;
    virtual std::vector<Position> get_all_positions() const = 0;

    virtual double calculate_total_exposure() const = 0;
    virtual double calculate_total_pnl() const = 0;
};

/**
 * @brief Risk check rules (each rule is independent)
 */
class IRiskRule {
public:
    virtual ~IRiskRule() = default;

    virtual RiskCheckResult check(
        const OrderRequest& order,
        const RiskMetrics& current_metrics,
        const RiskLimits& limits
    ) const = 0;

    virtual std::string name() const = 0;
};

} // namespace quantumliquidity::risk
