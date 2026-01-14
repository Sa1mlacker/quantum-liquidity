#pragma once

#include "quantumliquidity/common/types.hpp"
#include <functional>
#include <memory>
#include <map>
#include <vector>
#include <mutex>

namespace quantumliquidity::market_data {

/**
 * @brief Bar aggregator interface
 *
 * Converts tick data into OHLCV bars for various timeframes.
 * Supports time-based aggregation (1m, 5m, 15m, 30m, 1h, 4h, 1d).
 */
class IBarAggregator {
public:
    virtual ~IBarAggregator() = default;

    /**
     * @brief Callback invoked when a bar is completed
     *
     * Bar is considered complete when the time period ends
     * (e.g., at :00 for 1-minute bars, or at 00:00 UTC for daily bars)
     */
    using BarCallback = std::function<void(const Bar& bar)>;

    /**
     * @brief Process incoming tick data
     *
     * Updates all active bars for the instrument. When a bar completes,
     * the registered callback is invoked.
     *
     * @param tick Incoming market tick
     */
    virtual void process_tick(const Tick& tick) = 0;

    /**
     * @brief Register callback for bar completion
     *
     * @param callback Function to call when a bar completes
     */
    virtual void set_bar_callback(BarCallback callback) = 0;

    /**
     * @brief Enable aggregation for a specific timeframe
     *
     * @param instrument Instrument to aggregate
     * @param timeframe Target timeframe
     */
    virtual void enable_timeframe(const InstrumentID& instrument, TimeFrame timeframe) = 0;

    /**
     * @brief Disable aggregation for a specific timeframe
     *
     * @param instrument Instrument identifier
     * @param timeframe Target timeframe
     */
    virtual void disable_timeframe(const InstrumentID& instrument, TimeFrame timeframe) = 0;

    /**
     * @brief Force flush all incomplete bars
     *
     * Useful for end-of-session or shutdown. Incomplete bars
     * are finalized and callbacks are invoked.
     */
    virtual void flush_all() = 0;

    /**
     * @brief Get current (incomplete) bar for timeframe
     *
     * @param instrument Instrument identifier
     * @param timeframe Target timeframe
     * @return Current bar state, or nullopt if not active
     */
    virtual std::optional<Bar> get_current_bar(
        const InstrumentID& instrument,
        TimeFrame timeframe
    ) const = 0;

    /**
     * @brief Get aggregation statistics
     */
    struct Stats {
        uint64_t ticks_processed;
        uint64_t bars_completed;
        size_t active_instruments;
        size_t active_timeframes;
    };

    virtual Stats get_stats() const = 0;
};

/**
 * @brief Factory function to create bar aggregator
 *
 * @return Unique pointer to bar aggregator instance
 */
std::unique_ptr<IBarAggregator> create_bar_aggregator();

} // namespace quantumliquidity::market_data
