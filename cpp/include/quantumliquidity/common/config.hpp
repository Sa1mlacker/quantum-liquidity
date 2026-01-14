#pragma once

#include "quantumliquidity/persistence/database.hpp"
#include "quantumliquidity/persistence/redis.hpp"
#include "quantumliquidity/risk/risk_interface.hpp"
#include <string>
#include <vector>
#include <memory>

namespace quantumliquidity::config {

/**
 * @brief Application configuration
 *
 * Loaded from YAML file with environment variable overrides.
 */
struct AppConfig {
    // Environment
    std::string environment = "development";  // development, staging, production

    // Database
    persistence::DatabaseConfig database;

    // Redis
    persistence::RedisConfig redis;

    // Risk
    risk::RiskLimits risk_limits;

    // Logging
    struct LogConfig {
        std::string level = "INFO";
        std::string global_file;
        bool console_enabled = true;
        bool colored_console = true;

        struct ChannelConfig {
            std::string name;
            std::string file;
            std::string level;
        };
        std::vector<ChannelConfig> channels;
    } logging;

    // Market Data
    struct MarketDataConfig {
        struct ProviderConfig {
            std::string name;
            bool enabled = false;
            std::string api_key;
            std::string api_secret;
            std::vector<std::string> instruments;
            // Provider-specific fields (stored as key-value)
            std::map<std::string, std::string> extra;
        };
        std::vector<ProviderConfig> providers;

        // Replay settings (for backtesting)
        struct ReplayConfig {
            bool enabled = false;
            std::string start_date;
            std::string end_date;
            double speed_multiplier = 1.0;
        } replay;
    } market_data;

    // Strategies
    struct StrategyConfig {
        std::string id;
        std::string type;
        bool enabled = true;
        std::string mode = "paper";  // paper or live
        std::vector<std::string> instruments;
        std::map<std::string, std::string> parameters;
    };
    std::vector<StrategyConfig> strategies;
};

/**
 * @brief Configuration loader
 */
class ConfigLoader {
public:
    // Load from YAML file
    static AppConfig load_from_file(const std::string& filepath);

    // Load from YAML string
    static AppConfig load_from_string(const std::string& yaml_content);

    // Apply environment variable overrides
    static void apply_env_overrides(AppConfig& config);

    // Validate configuration
    static bool validate(const AppConfig& config, std::string& error_message);

private:
    ConfigLoader() = default;
};

} // namespace quantumliquidity::config
