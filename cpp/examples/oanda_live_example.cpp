/**
 * @file oanda_live_example.cpp
 * @brief Example of live market data streaming from OANDA
 *
 * This example demonstrates:
 * - OANDA v20 API connection
 * - Live tick streaming
 * - Bar aggregation from live ticks
 * - PostgreSQL persistence
 * - Redis publishing
 *
 * Prerequisites:
 * 1. Create OANDA practice (demo) account: https://www.oanda.com/register/
 * 2. Get your API token from: https://www.oanda.com/account/tpa/personal_token
 * 3. Get your account ID from OANDA dashboard
 * 4. Set environment variables:
 *    export OANDA_API_TOKEN="your-token-here"
 *    export OANDA_ACCOUNT_ID="your-account-id"
 */

#include "quantumliquidity/market_data/oanda_feed.hpp"
#include "quantumliquidity/market_data/feed_manager.hpp"
#include "quantumliquidity/market_data/bar_aggregator.hpp"
#include "quantumliquidity/persistence/database.hpp"
#include "quantumliquidity/persistence/redis.hpp"
#include "quantumliquidity/common/logger.hpp"
#include "quantumliquidity/common/config.hpp"

#include <iostream>
#include <csignal>
#include <atomic>

using namespace quantumliquidity;
using namespace quantumliquidity::market_data;
using namespace quantumliquidity::persistence;

std::atomic<bool> running(true);

void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down gracefully..." << std::endl;
    running = false;
}

int main(int argc, char** argv) {
    // Set up signal handlers for graceful shutdown
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Initialize logging
    Logger::initialize();
    Logger::add_console_sink(true);
    Logger::set_global_level(Logger::Level::INFO);

    Logger::info("system", "=== OANDA Live Market Data Example ===");

    // Get OANDA credentials from environment
    const char* api_token = std::getenv("OANDA_API_TOKEN");
    const char* account_id = std::getenv("OANDA_ACCOUNT_ID");

    if (!api_token || !account_id) {
        Logger::critical("system", "OANDA credentials not found!");
        Logger::critical("system", "Please set OANDA_API_TOKEN and OANDA_ACCOUNT_ID environment variables");
        Logger::critical("system", "");
        Logger::critical("system", "How to get credentials:");
        Logger::critical("system", "1. Create account: https://www.oanda.com/register/");
        Logger::critical("system", "2. Get API token: https://www.oanda.com/account/tpa/personal_token");
        Logger::critical("system", "3. Get account ID from dashboard");
        Logger::critical("system", "");
        Logger::critical("system", "Then run:");
        Logger::critical("system", "  export OANDA_API_TOKEN=\"your-token\"");
        Logger::critical("system", "  export OANDA_ACCOUNT_ID=\"your-account-id\"");
        return 1;
    }

    try {
        // Load configuration
        config::AppConfig config;
        if (argc > 1) {
            config = config::ConfigLoader::load_from_file(argv[1]);
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

        // Create database connection pool
        Logger::info("system", "Initializing database connection pool");
        auto db_pool = create_connection_pool(config.database);

        // Create time-series writer
        Logger::info("system", "Creating time-series writer");
        auto ts_writer = create_time_series_writer(db_pool, 1000, 1000);

        // Create Redis publisher
        Logger::info("system", "Creating Redis publisher");
        auto redis_pub = create_redis_publisher(config.redis);

        // Create bar aggregator
        Logger::info("system", "Creating bar aggregator");
        auto bar_agg = create_bar_aggregator();

        // Create feed manager
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
            TimeFrame::MIN_15,
            TimeFrame::HOUR_1
        };
        fm_config.enable_db_persistence = true;
        fm_config.enable_redis_publishing = true;
        fm_config.enable_bar_aggregation = true;

        auto feed_manager = create_feed_manager(std::move(fm_config));

        // Create OANDA feed
        Logger::info("system", "Creating OANDA feed");
        OANDAFeed::Config oanda_config;
        oanda_config.api_token = api_token;
        oanda_config.account_id = account_id;
        oanda_config.use_practice = true;  // Use demo account
        oanda_config.feed_name = "OANDA_Live";

        auto oanda_feed = std::make_shared<OANDAFeed>(oanda_config);

        // Register feed with manager
        feed_manager->add_feed(oanda_feed);

        // Subscribe to major FX pairs
        Logger::info("system", "Subscribing to instruments:");
        std::vector<std::string> instruments = {
            "EUR/USD",
            "GBP/USD",
            "USD/JPY",
            "AUD/USD",
            "USD/CHF"
        };

        for (const auto& instrument : instruments) {
            Logger::info("system", "  - " + instrument);
            feed_manager->subscribe_instrument(instrument);
        }

        // Start feed manager
        Logger::info("system", "Starting feed manager");
        feed_manager->start();

        Logger::info("system", "=== Live streaming started ===");
        Logger::info("system", "Press Ctrl+C to stop");
        Logger::info("system", "");

        // Main loop - print statistics every 10 seconds
        auto last_stats_time = std::chrono::steady_clock::now();
        
        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));

            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_stats_time);

            if (elapsed.count() >= 10) {
                auto stats = feed_manager->get_stats();
                
                Logger::info("system", "=== Statistics (last 10s) ===");
                Logger::info("system", "Ticks received: " + std::to_string(stats.ticks_received));
                Logger::info("system", "Ticks written: " + std::to_string(stats.ticks_written));
                Logger::info("system", "Bars completed: " + std::to_string(stats.bars_completed));
                Logger::info("system", "Bars written: " + std::to_string(stats.bars_written));
                Logger::info("system", "Redis publishes: " + std::to_string(stats.redis_publishes));
                Logger::info("system", "Errors: " + std::to_string(stats.error_count));
                Logger::info("system", "");

                last_stats_time = now;
            }
        }

        // Stop feed manager
        Logger::info("system", "Stopping feed manager");
        feed_manager->stop();

        // Final statistics
        auto stats = feed_manager->get_stats();
        Logger::info("system", "=== Final Statistics ===");
        Logger::info("system", "Total ticks received: " + std::to_string(stats.ticks_received));
        Logger::info("system", "Total ticks written: " + std::to_string(stats.ticks_written));
        Logger::info("system", "Total bars completed: " + std::to_string(stats.bars_completed));
        Logger::info("system", "Total bars written: " + std::to_string(stats.bars_written));
        Logger::info("system", "Total Redis publishes: " + std::to_string(stats.redis_publishes));
        Logger::info("system", "Total errors: " + std::to_string(stats.error_count));

        Logger::info("system", "=== Shutdown complete ===");

    } catch (const std::exception& e) {
        Logger::critical("system", "Fatal error: " + std::string(e.what()));
        return 1;
    }

    Logger::shutdown();
    return 0;
}
