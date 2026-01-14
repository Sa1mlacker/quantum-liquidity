#pragma once

#include "quantumliquidity/common/types.hpp"
#include <functional>
#include <memory>
#include <vector>

namespace quantumliquidity::market_data {

/**
 * @brief Abstract interface for market data feeds
 *
 * Implementations connect to broker/exchange APIs and deliver
 * normalized market data through callbacks.
 */
class IMarketDataFeed {
public:
    virtual ~IMarketDataFeed() = default;

    // Subscription management
    virtual void subscribe_ticks(const InstrumentID& instrument) = 0;
    virtual void subscribe_bars(const InstrumentID& instrument, TimeFrame tf) = 0;
    virtual void subscribe_depth(const InstrumentID& instrument, int levels = 10) = 0;

    virtual void unsubscribe_ticks(const InstrumentID& instrument) = 0;
    virtual void unsubscribe_bars(const InstrumentID& instrument, TimeFrame tf) = 0;
    virtual void unsubscribe_depth(const InstrumentID& instrument) = 0;

    // Connection lifecycle
    virtual void connect() = 0;
    virtual void disconnect() = 0;
    virtual bool is_connected() const = 0;

    // Callbacks (observer pattern)
    using TickCallback = std::function<void(const Tick&)>;
    using BarCallback = std::function<void(const Bar&)>;
    using DepthCallback = std::function<void(const DepthUpdate&)>;
    using ErrorCallback = std::function<void(const std::string& error)>;

    virtual void set_tick_callback(TickCallback callback) = 0;
    virtual void set_bar_callback(BarCallback callback) = 0;
    virtual void set_depth_callback(DepthCallback callback) = 0;
    virtual void set_error_callback(ErrorCallback callback) = 0;

    // Metadata
    virtual std::string name() const = 0;
    virtual std::vector<InstrumentInfo> available_instruments() const = 0;
};

/**
 * @brief Market data provider abstraction (broker-specific)
 *
 * Each broker (Darwinex, IC Markets, IB) implements this interface.
 */
class IMarketDataProvider {
public:
    virtual ~IMarketDataProvider() = default;

    // Factory method
    virtual std::unique_ptr<IMarketDataFeed> create_feed() = 0;

    // Provider info
    virtual std::string provider_name() const = 0;
    virtual std::vector<std::string> supported_asset_classes() const = 0;
};

/**
 * @brief Replay feed for backtesting
 *
 * Reads historical data from database and plays it back as if live.
 */
class IReplayFeed : public IMarketDataFeed {
public:
    // Replay control
    virtual void set_replay_speed(double multiplier) = 0;  // 1.0 = real-time
    virtual void set_replay_range(Timestamp start, Timestamp end) = 0;

    virtual void start_replay() = 0;
    virtual void pause_replay() = 0;
    virtual void resume_replay() = 0;
    virtual void stop_replay() = 0;

    virtual Timestamp current_replay_time() const = 0;
    virtual bool replay_finished() const = 0;
};

} // namespace quantumliquidity::market_data
