/**
 * @file market_data_daemon.cpp
 * @brief Automatic market data daemon - reads config and starts all feeds
 *
 * This daemon:
 * - Reads config/market_data.yaml
 * - Automatically creates and starts all configured feeds
 * - Subscribes to all instruments from config
 * - No manual coding required - just edit YAML!
 *
 * Usage:
 *   ./market_data_daemon [config_file]
 *   ./market_data_daemon config/market_data.yaml
 */

#include "quantumliquidity/market_data/oanda_feed.hpp"
#include "quantumliquidity/market_data/feed_manager.hpp"
#include "quantumliquidity/market_data/bar_aggregator.hpp"
#include "quantumliquidity/persistence/database.hpp"
#include "quantumliquidity/persistence/redis.hpp"
#include "quantumliquidity/common/logger.hpp"

#include <yaml-cpp/yaml.h>
#include <iostream>
#include <csignal>
#include <atomic>
#include <fstream>

using namespace quantumliquidity;
using namespace quantumliquidity::market_data;
using namespace quantumliquidity::persistence;

std::atomic<bool> running(true);

void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", shutting down..." << std::endl;
    running = false;
}

// Helper: expand environment variables in string
std::string expand_env(const std::string& str) {
    if (str.size() < 4 || str.substr(0, 2) != "${" || str.back() != '}') {
        return str;
    }
    
    std::string var_name = str.substr(2, str.size() - 3);
    const char* value = std::getenv(var_name.c_str());
    return value ? std::string(value) : "";
}

// Helper: parse timeframe string (1m, 5m, 1h, etc.)
TimeFrame parse_timeframe(const std::string& tf_str) {
    if (tf_str == "1m") return TimeFrame::MIN_1;
    if (tf_str == "5m") return TimeFrame::MIN_5;
    if (tf_str == "15m") return TimeFrame::MIN_15;
    if (tf_str == "30m") return TimeFrame::MIN_30;
    if (tf_str == "1h") return TimeFrame::HOUR_1;
    if (tf_str == "4h") return TimeFrame::HOUR_4;
    if (tf_str == "1d") return TimeFrame::DAY_1;
    
    Logger::warning("system", "Unknown timeframe: " + tf_str + ", defaulting to 1m");
    return TimeFrame::MIN_1;
}

int main(int argc, char** argv) {
    // Signal handlers
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    // Initialize logging
    Logger::initialize();
    Logger::add_console_sink(true);
    Logger::set_global_level(Logger::Level::INFO);

    Logger::info("system", "=== QuantumLiquidity Market Data Daemon ===");
    Logger::info("system", "Automatic feed configuration from YAML");
    Logger::info("system", "");

    // Load config file
    std::string config_file = argc > 1 ? argv[1] : "config/market_data.yaml";
    
    if (!std::ifstream(config_file).good()) {
        Logger::critical("system", "Config file not found: " + config_file);
        Logger::critical("system", "");
        Logger::critical("system", "Create config file or specify path:");
        Logger::critical("system", "  ./market_data_daemon config/market_data.yaml");
        return 1;
    }

    Logger::info("system", "Loading configuration: " + config_file);
    
    YAML::Node config;
    try {
        config = YAML::LoadFile(config_file);
    } catch (const std::exception& e) {
        Logger::critical("system", "Failed to parse YAML: " + std::string(e.what()));
        return 1;
    }

    try {
        // Create database pool (TODO: read from config)
        DatabaseConfig db_config;
        db_config.host = std::getenv("DATABASE_HOST") ? std::getenv("DATABASE_HOST") : "localhost";
        db_config.port = std::getenv("DATABASE_PORT") ? std::stoi(std::getenv("DATABASE_PORT")) : 5432;
        db_config.database = std::getenv("DATABASE_NAME") ? std::getenv("DATABASE_NAME") : "quantumliquidity";
        db_config.user = std::getenv("DATABASE_USER") ? std::getenv("DATABASE_USER") : "quantumliquidity";
        db_config.password = std::getenv("DATABASE_PASSWORD") ? std::getenv("DATABASE_PASSWORD") : "";
        db_config.pool_size = 10;

        Logger::info("system", "Connecting to database: " + db_config.database + "@" + db_config.host);
        auto db_pool = create_connection_pool(db_config);

        // Create time-series writer
        int batch_size = config["persistence"]["database"]["batch_size"].as<int>(1000);
        int flush_interval = config["persistence"]["database"]["flush_interval_ms"].as<int>(1000);
        auto ts_writer = create_time_series_writer(db_pool, batch_size, flush_interval);

        // Create Redis publisher
        RedisConfig redis_config;
        redis_config.host = std::getenv("REDIS_HOST") ? std::getenv("REDIS_HOST") : "localhost";
        redis_config.port = std::getenv("REDIS_PORT") ? std::stoi(std::getenv("REDIS_PORT")) : 6379;
        redis_config.db = 0;

        Logger::info("system", "Connecting to Redis: " + redis_config.host);
        auto redis_pub = create_redis_publisher(redis_config);

        // Create bar aggregator
        auto bar_agg = create_bar_aggregator();

        // Parse timeframes from config
        std::vector<TimeFrame> timeframes;
        if (config["aggregation"]["timeframes"]) {
            for (const auto& tf_node : config["aggregation"]["timeframes"]) {
                timeframes.push_back(parse_timeframe(tf_node.as<std::string>()));
            }
        } else {
            // Default timeframes
            timeframes = {TimeFrame::MIN_1, TimeFrame::MIN_5, TimeFrame::MIN_15, TimeFrame::HOUR_1};
        }

        // Create feed manager
        FeedManagerConfig fm_config;
        fm_config.db_writer = ts_writer;
        fm_config.redis_publisher = redis_pub;
        fm_config.bar_aggregator = bar_agg;
        fm_config.tick_channel = config["persistence"]["redis"]["channels"]["ticks"].as<std::string>("market.ticks");
        fm_config.bar_channel = config["persistence"]["redis"]["channels"]["bars"].as<std::string>("market.bars");
        fm_config.default_timeframes = timeframes;
        fm_config.enable_db_persistence = config["persistence"]["database"]["enabled"].as<bool>(true);
        fm_config.enable_redis_publishing = config["persistence"]["redis"]["enabled"].as<bool>(true);
        fm_config.enable_bar_aggregation = config["aggregation"]["enabled"].as<bool>(true);

        auto feed_manager = create_feed_manager(std::move(fm_config));

        // Process each feed from config
        Logger::info("system", "");
        Logger::info("system", "=== Configuring Feeds ===");
        
        int total_instruments = 0;
        
        for (const auto& feed_config : config["feeds"]) {
            std::string feed_name = feed_config["name"].as<std::string>();
            std::string feed_type = feed_config["type"].as<std::string>();
            bool enabled = feed_config["enabled"].as<bool>(false);

            if (!enabled) {
                Logger::info("system", "Feed disabled: " + feed_name + " (" + feed_type + ")");
                continue;
            }

            Logger::info("system", "");
            Logger::info("system", "Configuring feed: " + feed_name + " (" + feed_type + ")");

            // Create feed based on type
            if (feed_type == "oanda") {
                // OANDA feed
                OANDAFeed::Config oanda_config;
                oanda_config.api_token = expand_env(feed_config["credentials"]["api_token"].as<std::string>());
                oanda_config.account_id = expand_env(feed_config["credentials"]["account_id"].as<std::string>());
                oanda_config.use_practice = feed_config["credentials"]["use_practice"].as<bool>(true);
                oanda_config.feed_name = feed_name;

                if (oanda_config.api_token.empty() || oanda_config.account_id.empty()) {
                    Logger::warning("system", "OANDA credentials not found, skipping feed: " + feed_name);
                    Logger::warning("system", "Set OANDA_API_TOKEN and OANDA_ACCOUNT_ID environment variables");
                    continue;
                }

                auto feed = std::make_shared<OANDAFeed>(oanda_config);
                feed_manager->add_feed(feed);

                // Subscribe to instruments
                if (feed_config["instruments"]) {
                    for (const auto& instrument : feed_config["instruments"]) {
                        std::string inst_name = instrument.as<std::string>();
                        feed_manager->subscribe_instrument(inst_name);
                        total_instruments++;
                    }
                }

                Logger::info("system", "✓ OANDA feed configured with " + 
                            std::to_string(feed_config["instruments"].size()) + " instruments");
            }
            else if (feed_type == "polygon") {
                Logger::warning("system", "Polygon.io feed not yet implemented");
                // TODO: Implement Polygon feed
            }
            else if (feed_type == "alphavantage") {
                Logger::warning("system", "Alpha Vantage feed not yet implemented");
                // TODO: Implement Alpha Vantage feed
            }
            else {
                Logger::warning("system", "Unknown feed type: " + feed_type);
            }
        }

        Logger::info("system", "");
        Logger::info("system", "=== Summary ===");
        Logger::info("system", "Total instruments subscribed: " + std::to_string(total_instruments));
        Logger::info("system", "Timeframes: " + std::to_string(timeframes.size()));
        Logger::info("system", "Database persistence: " + 
                    std::string(fm_config.enable_db_persistence ? "enabled" : "disabled"));
        Logger::info("system", "Redis publishing: " + 
                    std::string(fm_config.enable_redis_publishing ? "enabled" : "disabled"));
        Logger::info("system", "");

        // Start feed manager
        Logger::info("system", "Starting feed manager...");
        feed_manager->start();

        Logger::info("system", "");
        Logger::info("system", "=== Streaming Started ===");
        Logger::info("system", "All feeds are now live!");
        Logger::info("system", "Press Ctrl+C to stop");
        Logger::info("system", "");

        // Main loop - print statistics
        auto last_stats_time = std::chrono::steady_clock::now();
        
        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));

            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_stats_time);

            if (elapsed.count() >= 30) {  // Every 30 seconds
                auto stats = feed_manager->get_stats();
                
                Logger::info("system", "=== Stats (last 30s) ===");
                Logger::info("system", "Ticks: " + std::to_string(stats.ticks_received) + 
                            " received, " + std::to_string(stats.ticks_written) + " written");
                Logger::info("system", "Bars: " + std::to_string(stats.bars_completed) + 
                            " completed, " + std::to_string(stats.bars_written) + " written");
                Logger::info("system", "Redis: " + std::to_string(stats.redis_publishes) + " publishes");
                Logger::info("system", "Errors: " + std::to_string(stats.error_count));
                Logger::info("system", "");

                last_stats_time = now;
            }
        }

        // Graceful shutdown
        Logger::info("system", "Stopping feed manager...");
        feed_manager->stop();

        auto stats = feed_manager->get_stats();
        Logger::info("system", "");
        Logger::info("system", "=== Final Statistics ===");
        Logger::info("system", "Total ticks: " + std::to_string(stats.ticks_received));
        Logger::info("system", "Total bars: " + std::to_string(stats.bars_completed));
        Logger::info("system", "Total Redis publishes: " + std::to_string(stats.redis_publishes));
        Logger::info("system", "Total errors: " + std::to_string(stats.error_count));
        Logger::info("system", "");
        Logger::info("system", "=== Shutdown Complete ===");

    } catch (const std::exception& e) {
        Logger::critical("system", "Fatal error: " + std::string(e.what()));
        return 1;
    }

    Logger::shutdown();
    return 0;
}
