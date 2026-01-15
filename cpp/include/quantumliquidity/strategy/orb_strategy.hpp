/**
 * Opening Range Breakout Strategy
 * Trades breakouts from the opening range
 */

#pragma once

#include "quantumliquidity/strategy/strategy.hpp"
#include "quantumliquidity/analytics/orb_analyzer.hpp"
#include <map>
#include <chrono>

namespace quantumliquidity::strategy {

struct ORBConfig : public StrategyConfig {
    int period_minutes = 30;        // Opening range period
    double breakout_threshold = 0.0; // Minimum breakout distance
    int max_positions = 1;          // Max simultaneous positions
    double position_size = 1.0;     // Size per trade
    bool trade_high_breakout = true;
    bool trade_low_breakout = true;

    // Time filters
    int session_start_hour = 9;     // Market open (e.g., 9:30 AM)
    int session_start_minute = 30;
    int session_end_hour = 16;      // Market close
    int session_end_minute = 0;
};

class ORBStrategy : public Strategy {
public:
    explicit ORBStrategy(const ORBConfig& config);

    // Lifecycle
    void on_start() override;
    void on_stop() override;

    // Market data
    void on_tick(const market_data::Tick& tick) override;
    void on_bar(const market_data::OHLCV& bar) override;

    // Execution
    void on_fill(const execution::Fill& fill) override;
    void on_order_update(const execution::Order& order) override;

private:
    ORBConfig orb_config_;
    analytics::ORBAnalyzer analyzer_;

    struct InstrumentState {
        double or_high = 0.0;
        double or_low = 0.0;
        bool or_calculated = false;
        bool high_breakout_taken = false;
        bool low_breakout_taken = false;
        std::chrono::system_clock::time_point session_start;
        std::chrono::system_clock::time_point or_end;
    };

    std::map<std::string, InstrumentState> instrument_states_;

    // Helper methods
    void calculate_opening_range(const std::string& instrument);
    void check_breakout(const std::string& instrument, double price);
    bool is_trading_hours() const;
    void reset_daily_state();
};

} // namespace quantumliquidity::strategy
