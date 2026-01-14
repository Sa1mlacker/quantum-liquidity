/**
 * @file market_data_example.cpp
 * @brief Example demonstrating Phase 2: Market Data Gateway
 *
 * This example shows:
 * - CSV feed reading tick data
 * - Bar aggregation (ticks → OHLCV bars)
 * - Feed manager orchestration
 * - PostgreSQL persistence
 * - Redis pub/sub
 */

#include "quantumliquidity/market_data/csv_feed.hpp"
#include "quantumliquidity/market_data/feed_manager.hpp"
#include "quantumliquidity/market_data/bar_aggregator.hpp"
#include "quantumliquidity/persistence/database.hpp"
#include "quantumliquidity/persistence/redis.hpp"
#include "quantumliquidity/common/logger.hpp"
#include "quantumliquidity/common/config.hpp"

#include <iostream>
#include <thread>
#include <chrono>

using namespace quantumliquidity;
using namespace quantumliquidity::market_data;
using namespace quantumliquidity::persistence;
using namespace quantumliquidity::config;

int main(int argc, char** argv) {
    // Initialize logging
    Logger::initialize();
    Logger::add_console_sink(true);
    Logger::set_global_level(Logger::Level::INFO);

    Logger::info("system", "=== Market Data Gateway Example ===");

    try {
        // 1. Load configuration
        AppConfig config;
        if (argc > 1) {
            config = ConfigLoader::load_from_file(argv[1]);
        } else {
            Logger::info("system", "Using default configuration");
            config.database.host = "localhost";
            config.database.port = 5432;
            config.database.database = "quantumliquidity";
            config.database.user = "quantumliquidity";
            config.database.password = "";
            config.database.pool_size = 5;

            config.redis.host = "localhost";
            config.redis.port = 6379;
            config.redis.db = 0;
        }

        // 2. Create database connection pool
        Logger::info("system", "Initializing database connection pool");
        auto db_pool = create_connection_pool(config.database);

        // 3. Create time-series writer
        Logger::info("system", "Creating time-series writer");
        auto ts_writer = create_time_series_writer(
            db_pool,
            1000,  // batch_size
            1000   // flush_interval_ms
        );

        // Start writer background thread
        // Note: TimeSeriesWriter will need a start() method
        // For now, it auto-starts on first write

        // 4. Create Redis publisher
        Logger::info("system", "Creating Redis publisher");
        auto redis_pub = create_redis_publisher(config.redis);

        // 5. Create bar aggregator
        Logger::info("system", "Creating bar aggregator");
        auto bar_agg = create_bar_aggregator();

        // 6. Create feed manager
        Logger::info("system", "Creating feed manager");
        FeedManagerConfig fm_config;
        fm_config.db_writer = ts_writer;
        fm_config.redis_publisher = redis_pub;
        fm_config.bar_aggregator = bar_agg;
        fm_config.tick_channel = "market.ticks";
        fm_config.bar_channel = "market.bars";
        fm_config.default_timeframes = {
            TimeFrame::MIN_1,
            TimeFrame::MIN_5,
            TimeFrame::MIN_15
        };
        fm_config.enable_db_persistence = true;
        fm_config.enable_redis_publishing = true;
        fm_config.enable_bar_aggregation = true;

        auto feed_manager = create_feed_manager(std::move(fm_config));

        // 7. Create CSV feed
        Logger::info("system", "Creating CSV feed");
        CSVFeed::Config csv_config;
        csv_config.csv_filepath = argc > 2 ? argv[2] : "../data/sample_ticks.csv";
        csv_config.feed_name = "CSV_Demo";
        csv_config.replay_speed = 0;  // No delay (fastest)
        csv_config.loop = false;

        auto csv_feed = std::make_shared<CSVFeed>(csv_config);

        // 8. Register feed with manager
        feed_manager->add_feed(csv_feed);

        // 9. Subscribe to instruments
        Logger::info("system", "Subscribing to EUR/USD and GBP/USD");
        feed_manager->subscribe_instrument("EUR/USD");
        feed_manager->subscribe_instrument("GBP/USD");

        // 10. Start feed manager
        Logger::info("system", "Starting feed manager");
        feed_manager->start();

        // Wait for replay to complete
        Logger::info("system", "Replaying market data...");
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // 11. Print statistics
        auto stats = feed_manager->get_stats();
        Logger::info("system", "=== Statistics ===");
        Logger::info("system", "Ticks received: " + std::to_string(stats.ticks_received));
        Logger::info("system", "Ticks written: " + std::to_string(stats.ticks_written));
        Logger::info("system", "Bars completed: " + std::to_string(stats.bars_completed));
        Logger::info("system", "Bars written: " + std::to_string(stats.bars_written));
        Logger::info("system", "Redis publishes: " + std::to_string(stats.redis_publishes));
        Logger::info("system", "Errors: " + std::to_string(stats.error_count));

        // 12. Stop feed manager
        Logger::info("system", "Stopping feed manager");
        feed_manager->stop();

        Logger::info("system", "=== Example completed successfully ===");

    } catch (const std::exception& e) {
        Logger::critical("system", "Fatal error: " + std::string(e.what()));
        return 1;
    }

    Logger::shutdown();
    return 0;
}
