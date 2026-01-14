#include "quantumliquidity/market_data/bar_aggregator.hpp"
#include "quantumliquidity/common/logger.hpp"
#include <chrono>
#include <algorithm>
#include <sstream>

namespace quantumliquidity::market_data {

namespace {

/**
 * @brief Helper to get timeframe duration in seconds
 */
int64_t timeframe_to_seconds(TimeFrame tf) {
    switch (tf) {
        case TimeFrame::MIN_1: return 60;
        case TimeFrame::MIN_5: return 300;
        case TimeFrame::MIN_15: return 900;
        case TimeFrame::MIN_30: return 1800;
        case TimeFrame::HOUR_1: return 3600;
        case TimeFrame::HOUR_4: return 14400;
        case TimeFrame::DAY_1: return 86400;
        default: return 60;
    }
}

/**
 * @brief Helper to align timestamp to bar boundary
 *
 * E.g., for 5-minute bars, rounds down to nearest 5-minute mark
 */
Timestamp align_to_timeframe(Timestamp ts, TimeFrame tf) {
    auto duration = std::chrono::system_clock::time_point(ts).time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();

    int64_t tf_seconds = timeframe_to_seconds(tf);
    int64_t aligned_seconds = (seconds / tf_seconds) * tf_seconds;

    return Timestamp(std::chrono::seconds(aligned_seconds));
}

/**
 * @brief Helper to get next bar boundary
 */
Timestamp next_bar_boundary(Timestamp current, TimeFrame tf) {
    auto aligned = align_to_timeframe(current, tf);
    auto duration = std::chrono::system_clock::time_point(aligned).time_since_epoch();
    auto seconds = std::chrono::duration_cast<std::chrono::seconds>(duration).count();

    int64_t tf_seconds = timeframe_to_seconds(tf);
    int64_t next_seconds = seconds + tf_seconds;

    return Timestamp(std::chrono::seconds(next_seconds));
}

/**
 * @brief Format timeframe as string for logging
 */
std::string timeframe_to_string(TimeFrame tf) {
    switch (tf) {
        case TimeFrame::MIN_1: return "1m";
        case TimeFrame::MIN_5: return "5m";
        case TimeFrame::MIN_15: return "15m";
        case TimeFrame::MIN_30: return "30m";
        case TimeFrame::HOUR_1: return "1h";
        case TimeFrame::HOUR_4: return "4h";
        case TimeFrame::DAY_1: return "1d";
        default: return "unknown";
    }
}

} // anonymous namespace

/**
 * @brief Tracks state of a single bar being built
 */
struct BarState {
    Bar bar;
    Timestamp boundary;  // Next bar boundary (when this bar should close)
    bool initialized;

    BarState() : initialized(false) {}

    void reset(const InstrumentID& instrument, TimeFrame tf, Timestamp ts) {
        bar.instrument = instrument;
        bar.timeframe = tf;
        bar.timestamp = align_to_timeframe(ts, tf);
        bar.open = 0.0;
        bar.high = 0.0;
        bar.low = 0.0;
        bar.close = 0.0;
        bar.volume = 0.0;
        bar.tick_count = 0;
        boundary = next_bar_boundary(ts, tf);
        initialized = false;
    }

    void update_from_tick(const Tick& tick) {
        Price mid = (tick.bid + tick.ask) / 2.0;

        if (!initialized) {
            bar.open = mid;
            bar.high = mid;
            bar.low = mid;
            bar.close = mid;
            initialized = true;
        } else {
            bar.high = std::max(bar.high, mid);
            bar.low = std::min(bar.low, mid);
            bar.close = mid;
        }

        // Volume estimation (use trade volume if available, otherwise estimate from spread)
        if (tick.last_trade_size.has_value()) {
            bar.volume += *tick.last_trade_size;
        }

        bar.tick_count++;
    }
};

/**
 * @brief Key for identifying a unique bar (instrument + timeframe)
 */
struct BarKey {
    InstrumentID instrument;
    TimeFrame timeframe;

    bool operator<(const BarKey& other) const {
        if (instrument != other.instrument) {
            return instrument < other.instrument;
        }
        return static_cast<int>(timeframe) < static_cast<int>(other.timeframe);
    }
};

/**
 * @brief Bar aggregator implementation
 */
class BarAggregator : public IBarAggregator {
public:
    BarAggregator() : stats_{0, 0, 0, 0} {
        Logger::info("market_data", "Bar aggregator initialized");
    }

    ~BarAggregator() override {
        flush_all();
        Logger::info("market_data", "Bar aggregator shutdown");
    }

    void process_tick(const Tick& tick) override {
        std::lock_guard<std::mutex> lock(mutex_);

        stats_.ticks_processed++;

        // Find all active bars for this instrument
        for (auto& [key, state] : active_bars_) {
            if (key.instrument != tick.instrument) {
                continue;
            }

            // Check if we need to finalize the current bar
            if (tick.timestamp >= state.boundary) {
                // Complete the current bar
                if (state.initialized) {
                    finalize_bar(state.bar);
                }

                // Start a new bar
                state.reset(key.instrument, key.timeframe, tick.timestamp);
            }

            // Update the current bar with this tick
            state.update_from_tick(tick);
        }
    }

    void set_bar_callback(BarCallback callback) override {
        std::lock_guard<std::mutex> lock(mutex_);
        callback_ = std::move(callback);
    }

    void enable_timeframe(const InstrumentID& instrument, TimeFrame timeframe) override {
        std::lock_guard<std::mutex> lock(mutex_);

        BarKey key{instrument, timeframe};

        if (active_bars_.count(key)) {
            Logger::warning("market_data",
                "Timeframe already enabled: " + instrument + " " + timeframe_to_string(timeframe));
            return;
        }

        active_bars_[key] = BarState();

        // Update stats
        update_stats_unlocked();

        Logger::info("market_data",
            "Enabled bar aggregation: " + instrument + " " + timeframe_to_string(timeframe));
    }

    void disable_timeframe(const InstrumentID& instrument, TimeFrame timeframe) override {
        std::lock_guard<std::mutex> lock(mutex_);

        BarKey key{instrument, timeframe};

        auto it = active_bars_.find(key);
        if (it == active_bars_.end()) {
            return;
        }

        // Flush the incomplete bar if needed
        if (it->second.initialized) {
            finalize_bar(it->second.bar);
        }

        active_bars_.erase(it);

        // Update stats
        update_stats_unlocked();

        Logger::info("market_data",
            "Disabled bar aggregation: " + instrument + " " + timeframe_to_string(timeframe));
    }

    void flush_all() override {
        std::lock_guard<std::mutex> lock(mutex_);

        for (auto& [key, state] : active_bars_) {
            if (state.initialized) {
                finalize_bar(state.bar);
            }
        }

        Logger::info("market_data", "Flushed all incomplete bars");
    }

    std::optional<Bar> get_current_bar(
        const InstrumentID& instrument,
        TimeFrame timeframe
    ) const override {
        std::lock_guard<std::mutex> lock(mutex_);

        BarKey key{instrument, timeframe};
        auto it = active_bars_.find(key);

        if (it == active_bars_.end() || !it->second.initialized) {
            return std::nullopt;
        }

        return it->second.bar;
    }

    Stats get_stats() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_;
    }

private:
    void finalize_bar(const Bar& bar) {
        stats_.bars_completed++;

        // Invoke callback if registered
        if (callback_) {
            callback_(bar);
        }

        Logger::debug("market_data",
            "Bar completed: " + bar.instrument + " " +
            timeframe_to_string(bar.timeframe) +
            " O:" + std::to_string(bar.open) +
            " H:" + std::to_string(bar.high) +
            " L:" + std::to_string(bar.low) +
            " C:" + std::to_string(bar.close) +
            " V:" + std::to_string(bar.volume) +
            " Ticks:" + std::to_string(bar.tick_count));
    }

    void update_stats_unlocked() {
        // Count unique instruments
        std::set<InstrumentID> instruments;
        for (const auto& [key, _] : active_bars_) {
            instruments.insert(key.instrument);
        }

        stats_.active_instruments = instruments.size();
        stats_.active_timeframes = active_bars_.size();
    }

    mutable std::mutex mutex_;
    BarCallback callback_;
    std::map<BarKey, BarState> active_bars_;
    Stats stats_;
};

// Factory function
std::unique_ptr<IBarAggregator> create_bar_aggregator() {
    return std::make_unique<BarAggregator>();
}

} // namespace quantumliquidity::market_data
