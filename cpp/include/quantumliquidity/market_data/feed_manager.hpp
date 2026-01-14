#pragma once

#include "quantumliquidity/market_data/feed_interface.hpp"
#include "quantumliquidity/market_data/bar_aggregator.hpp"
#include "quantumliquidity/persistence/database.hpp"
#include "quantumliquidity/persistence/redis.hpp"
#include <memory>
#include <vector>
#include <map>
#include <mutex>

namespace quantumliquidity::market_data {

/**
 * @brief Market data feed manager
 *
 * Orchestrates multiple market data feeds:
 * - Manages feed lifecycle (connect, subscribe, disconnect)
 * - Aggregates ticks into bars (via BarAggregator)
 * - Publishes market data to Redis event bus
 * - Persists ticks and bars to PostgreSQL
 */
class IFeedManager {
public:
    virtual ~IFeedManager() = default;

    /**
     * @brief Register a market data feed
     *
     * @param feed Shared pointer to feed implementation
     */
    virtual void add_feed(std::shared_ptr<IMarketDataFeed> feed) = 0;

    /**
     * @brief Remove a feed by name
     *
     * @param feed_name Name of the feed to remove
     */
    virtual void remove_feed(const std::string& feed_name) = 0;

    /**
     * @brief Subscribe to tick data for an instrument
     *
     * Automatically enables bar aggregation for configured timeframes.
     *
     * @param instrument Instrument identifier
     */
    virtual void subscribe_instrument(const InstrumentID& instrument) = 0;

    /**
     * @brief Unsubscribe from instrument
     *
     * @param instrument Instrument identifier
     */
    virtual void unsubscribe_instrument(const InstrumentID& instrument) = 0;

    /**
     * @brief Enable bar aggregation for timeframe
     *
     * @param instrument Instrument identifier
     * @param timeframe Target timeframe
     */
    virtual void enable_bars(const InstrumentID& instrument, TimeFrame timeframe) = 0;

    /**
     * @brief Disable bar aggregation for timeframe
     *
     * @param instrument Instrument identifier
     * @param timeframe Target timeframe
     */
    virtual void disable_bars(const InstrumentID& instrument, TimeFrame timeframe) = 0;

    /**
     * @brief Start all feeds
     *
     * Connects to all registered feeds and begins data flow.
     */
    virtual void start() = 0;

    /**
     * @brief Stop all feeds
     *
     * Disconnects from feeds and flushes pending data.
     */
    virtual void stop() = 0;

    /**
     * @brief Check if manager is running
     */
    virtual bool is_running() const = 0;

    /**
     * @brief Get list of active feeds
     */
    virtual std::vector<std::string> active_feeds() const = 0;

    /**
     * @brief Get aggregated statistics
     */
    struct Stats {
        uint64_t ticks_received;
        uint64_t ticks_written;
        uint64_t bars_completed;
        uint64_t bars_written;
        uint64_t redis_publishes;
        uint64_t error_count;
        size_t active_feeds;
        size_t subscribed_instruments;
    };

    virtual Stats get_stats() const = 0;
};

/**
 * @brief Configuration for feed manager
 */
struct FeedManagerConfig {
    // Database persistence
    std::shared_ptr<persistence::ITimeSeriesWriter> db_writer;

    // Redis publishing
    std::shared_ptr<persistence::IRedisPublisher> redis_publisher;

    // Bar aggregation
    std::shared_ptr<IBarAggregator> bar_aggregator;

    // Redis channels
    std::string tick_channel = "market.ticks";
    std::string bar_channel = "market.bars";

    // Default timeframes to enable
    std::vector<TimeFrame> default_timeframes = {
        TimeFrame::MIN_1,
        TimeFrame::MIN_5,
        TimeFrame::MIN_15,
        TimeFrame::HOUR_1,
        TimeFrame::DAY_1
    };

    // Enable/disable components
    bool enable_db_persistence = true;
    bool enable_redis_publishing = true;
    bool enable_bar_aggregation = true;
};

/**
 * @brief Factory function to create feed manager
 *
 * @param config Feed manager configuration
 * @return Unique pointer to feed manager instance
 */
std::unique_ptr<IFeedManager> create_feed_manager(FeedManagerConfig config);

} // namespace quantumliquidity::market_data
