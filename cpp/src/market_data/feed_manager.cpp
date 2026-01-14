#include "quantumliquidity/market_data/feed_manager.hpp"
#include "quantumliquidity/common/logger.hpp"
#include "quantumliquidity/common/utils.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>

namespace quantumliquidity::market_data {

/**
 * @brief Feed manager implementation
 */
class FeedManager : public IFeedManager {
public:
    explicit FeedManager(FeedManagerConfig config)
        : config_(std::move(config))
        , running_(false)
        , stats_{0, 0, 0, 0, 0, 0, 0, 0}
    {
        // Set up bar aggregator callback
        if (config_.bar_aggregator && config_.enable_bar_aggregation) {
            config_.bar_aggregator->set_bar_callback(
                [this](const Bar& bar) { on_bar_completed(bar); }
            );
        }

        Logger::info("market_data", "Feed manager initialized");
    }

    ~FeedManager() override {
        stop();
        Logger::info("market_data", "Feed manager shutdown");
    }

    void add_feed(std::shared_ptr<IMarketDataFeed> feed) override {
        std::lock_guard<std::mutex> lock(mutex_);

        std::string feed_name = feed->name();

        if (feeds_.count(feed_name)) {
            Logger::warning("market_data", "Feed already registered: " + feed_name);
            return;
        }

        // Set up callbacks
        feed->set_tick_callback([this](const Tick& tick) { on_tick_received(tick); });
        feed->set_error_callback([this](const std::string& error) { on_feed_error(error); });

        feeds_[feed_name] = feed;
        stats_.active_feeds++;

        Logger::info("market_data", "Added feed: " + feed_name);
    }

    void remove_feed(const std::string& feed_name) override {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = feeds_.find(feed_name);
        if (it == feeds_.end()) {
            return;
        }

        // Disconnect if connected
        if (it->second->is_connected()) {
            it->second->disconnect();
        }

        feeds_.erase(it);
        stats_.active_feeds--;

        Logger::info("market_data", "Removed feed: " + feed_name);
    }

    void subscribe_instrument(const InstrumentID& instrument) override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (subscribed_instruments_.count(instrument)) {
            Logger::warning("market_data", "Already subscribed to: " + instrument);
            return;
        }

        // Subscribe on all feeds
        for (auto& [name, feed] : feeds_) {
            if (feed->is_connected()) {
                feed->subscribe_ticks(instrument);
            }
        }

        subscribed_instruments_.insert(instrument);
        stats_.subscribed_instruments++;

        // Enable default timeframes for bar aggregation
        if (config_.enable_bar_aggregation && config_.bar_aggregator) {
            for (TimeFrame tf : config_.default_timeframes) {
                config_.bar_aggregator->enable_timeframe(instrument, tf);
            }
        }

        Logger::info("market_data", "Subscribed to instrument: " + instrument);
    }

    void unsubscribe_instrument(const InstrumentID& instrument) override {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = subscribed_instruments_.find(instrument);
        if (it == subscribed_instruments_.end()) {
            return;
        }

        // Unsubscribe from all feeds
        for (auto& [name, feed] : feeds_) {
            if (feed->is_connected()) {
                feed->unsubscribe_ticks(instrument);
            }
        }

        subscribed_instruments_.erase(it);
        stats_.subscribed_instruments--;

        // Disable bar aggregation
        if (config_.bar_aggregator) {
            for (TimeFrame tf : config_.default_timeframes) {
                config_.bar_aggregator->disable_timeframe(instrument, tf);
            }
        }

        Logger::info("market_data", "Unsubscribed from instrument: " + instrument);
    }

    void enable_bars(const InstrumentID& instrument, TimeFrame timeframe) override {
        if (!config_.bar_aggregator) {
            Logger::warning("market_data", "Bar aggregator not configured");
            return;
        }

        config_.bar_aggregator->enable_timeframe(instrument, timeframe);
    }

    void disable_bars(const InstrumentID& instrument, TimeFrame timeframe) override {
        if (!config_.bar_aggregator) {
            return;
        }

        config_.bar_aggregator->disable_timeframe(instrument, timeframe);
    }

    void start() override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (running_) {
            Logger::warning("market_data", "Feed manager already running");
            return;
        }

        // Connect all feeds
        for (auto& [name, feed] : feeds_) {
            try {
                if (!feed->is_connected()) {
                    feed->connect();
                }

                // Re-subscribe to instruments
                for (const auto& instrument : subscribed_instruments_) {
                    feed->subscribe_ticks(instrument);
                }

            } catch (const std::exception& e) {
                Logger::error("market_data",
                    "Failed to start feed " + name + ": " + e.what());
                stats_.error_count++;
            }
        }

        running_ = true;
        Logger::info("market_data", "Feed manager started");
    }

    void stop() override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!running_) {
            return;
        }

        running_ = false;

        // Flush bar aggregator
        if (config_.bar_aggregator) {
            config_.bar_aggregator->flush_all();
        }

        // Flush database writer
        if (config_.db_writer) {
            config_.db_writer->flush();
        }

        // Disconnect all feeds
        for (auto& [name, feed] : feeds_) {
            try {
                if (feed->is_connected()) {
                    feed->disconnect();
                }
            } catch (const std::exception& e) {
                Logger::error("market_data",
                    "Error disconnecting feed " + name + ": " + e.what());
            }
        }

        Logger::info("market_data", "Feed manager stopped");
    }

    bool is_running() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return running_;
    }

    std::vector<std::string> active_feeds() const override {
        std::lock_guard<std::mutex> lock(mutex_);

        std::vector<std::string> result;
        result.reserve(feeds_.size());

        for (const auto& [name, feed] : feeds_) {
            if (feed->is_connected()) {
                result.push_back(name);
            }
        }

        return result;
    }

    Stats get_stats() const override {
        std::lock_guard<std::mutex> lock(mutex_);

        // Update aggregator stats
        Stats result = stats_;
        if (config_.bar_aggregator) {
            auto agg_stats = config_.bar_aggregator->get_stats();
            result.bars_completed = agg_stats.bars_completed;
        }

        // Update writer stats
        if (config_.db_writer) {
            auto writer_stats = config_.db_writer->get_stats();
            result.ticks_written = writer_stats.ticks_written;
            result.bars_written = writer_stats.bars_written;
        }

        return result;
    }

private:
    /**
     * @brief Callback for incoming tick data
     */
    void on_tick_received(const Tick& tick) {
        std::lock_guard<std::mutex> lock(mutex_);
        stats_.ticks_received++;

        // Process through bar aggregator
        if (config_.enable_bar_aggregation && config_.bar_aggregator) {
            config_.bar_aggregator->process_tick(tick);
        }

        // Write to database
        if (config_.enable_db_persistence && config_.db_writer) {
            config_.db_writer->write_tick(tick);
        }

        // Publish to Redis
        if (config_.enable_redis_publishing && config_.redis_publisher) {
            publish_tick_to_redis(tick);
        }
    }

    /**
     * @brief Callback for completed bars
     */
    void on_bar_completed(const Bar& bar) {
        std::lock_guard<std::mutex> lock(mutex_);

        // Write to database
        if (config_.enable_db_persistence && config_.db_writer) {
            config_.db_writer->write_bar(bar);
        }

        // Publish to Redis
        if (config_.enable_redis_publishing && config_.redis_publisher) {
            publish_bar_to_redis(bar);
        }
    }

    /**
     * @brief Callback for feed errors
     */
    void on_feed_error(const std::string& error) {
        stats_.error_count++;
        Logger::error("market_data", "Feed error: " + error);
    }

    /**
     * @brief Publish tick to Redis as JSON
     */
    void publish_tick_to_redis(const Tick& tick) {
        try {
            nlohmann::json j;
            j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                tick.timestamp.time_since_epoch()).count();
            j["instrument"] = tick.instrument;
            j["bid"] = tick.bid;
            j["ask"] = tick.ask;
            j["bid_size"] = tick.bid_size;
            j["ask_size"] = tick.ask_size;

            if (tick.last_trade_price.has_value()) {
                j["last_price"] = *tick.last_trade_price;
            }
            if (tick.last_trade_size.has_value()) {
                j["last_size"] = *tick.last_trade_size;
            }

            config_.redis_publisher->publish(config_.tick_channel, j.dump());
            stats_.redis_publishes++;

        } catch (const std::exception& e) {
            Logger::error("market_data", "Failed to publish tick: " + std::string(e.what()));
            stats_.error_count++;
        }
    }

    /**
     * @brief Publish bar to Redis as JSON
     */
    void publish_bar_to_redis(const Bar& bar) {
        try {
            nlohmann::json j;
            j["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                bar.timestamp.time_since_epoch()).count();
            j["instrument"] = bar.instrument;
            j["timeframe"] = static_cast<int>(bar.timeframe);
            j["open"] = bar.open;
            j["high"] = bar.high;
            j["low"] = bar.low;
            j["close"] = bar.close;
            j["volume"] = bar.volume;
            j["tick_count"] = bar.tick_count;

            config_.redis_publisher->publish(config_.bar_channel, j.dump());
            stats_.redis_publishes++;

        } catch (const std::exception& e) {
            Logger::error("market_data", "Failed to publish bar: " + std::string(e.what()));
            stats_.error_count++;
        }
    }

    FeedManagerConfig config_;
    bool running_;

    mutable std::mutex mutex_;
    std::map<std::string, std::shared_ptr<IMarketDataFeed>> feeds_;
    std::set<InstrumentID> subscribed_instruments_;
    Stats stats_;
};

// Factory function
std::unique_ptr<IFeedManager> create_feed_manager(FeedManagerConfig config) {
    return std::make_unique<FeedManager>(std::move(config));
}

} // namespace quantumliquidity::market_data
