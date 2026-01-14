#include "quantumliquidity/common/config.hpp"
#include "quantumliquidity/common/logger.hpp"
#include <fstream>
#include <sstream>
#include <cstdlib>

// TODO: When yaml-cpp is available, replace with:
// #include <yaml-cpp/yaml.h>

namespace quantumliquidity::config {

namespace {

// Helper: get environment variable with default
std::string get_env(const std::string& name, const std::string& default_value = "") {
    const char* value = std::getenv(name.c_str());
    return value ? std::string(value) : default_value;
}

// Helper: parse simple YAML-like format (simplified, production should use yaml-cpp)
// Format: "key: value" or "  - item" for lists
std::map<std::string, std::string> parse_simple_yaml(const std::string& content) {
    std::map<std::string, std::string> result;
    std::istringstream stream(content);
    std::string line;

    while (std::getline(stream, line)) {
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // Find colon separator
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string key = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 1);

            // Trim whitespace
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            result[key] = value;
        }
    }

    return result;
}

} // anonymous namespace

AppConfig ConfigLoader::load_from_file(const std::string& filepath) {
    Logger::info("system", "Loading configuration from: " + filepath);

    std::ifstream file(filepath);
    if (!file.is_open()) {
        Logger::error("system", "Failed to open config file: " + filepath);
        throw std::runtime_error("Failed to open config file: " + filepath);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    return load_from_string(content);
}

AppConfig ConfigLoader::load_from_string(const std::string& yaml_content) {
    AppConfig config;

    // TODO: Real implementation with yaml-cpp
    // YAML::Node root = YAML::Load(yaml_content);
    //
    // config.environment = root["environment"].as<std::string>("development");
    //
    // // Database
    // if (root["database"]) {
    //     auto db = root["database"];
    //     config.database.host = db["host"].as<std::string>("localhost");
    //     config.database.port = db["port"].as<int>(5432);
    //     config.database.database = db["database"].as<std::string>("quantumliquidity");
    //     config.database.user = db["user"].as<std::string>("quantumliquidity");
    //     config.database.password = db["password"].as<std::string>("");
    //     config.database.pool_size = db["pool_size"].as<int>(10);
    // }
    //
    // // Redis
    // if (root["redis"]) {
    //     auto redis = root["redis"];
    //     config.redis.host = redis["host"].as<std::string>("localhost");
    //     config.redis.port = redis["port"].as<int>(6379);
    //     config.redis.db = redis["db"].as<int>(0);
    //     config.redis.password = redis["password"].as<std::string>("");
    // }
    //
    // // Risk
    // if (root["risk"]) {
    //     auto risk = root["risk"];
    //     config.risk_limits.max_position_value_per_instrument =
    //         risk["max_position_value_per_instrument"].as<double>(100000.0);
    //     config.risk_limits.max_total_exposure =
    //         risk["max_total_exposure"].as<double>(500000.0);
    //     config.risk_limits.max_daily_loss =
    //         risk["max_daily_loss"].as<double>(10000.0);
    //     config.risk_limits.max_leverage =
    //         risk["max_leverage"].as<double>(10.0);
    // }
    //
    // // Logging
    // if (root["logging"]) {
    //     auto logging = root["logging"];
    //     config.logging.level = logging["log_level"].as<std::string>("INFO");
    //     config.logging.global_file = logging["log_file"].as<std::string>("");
    //     config.logging.console_enabled = true;
    // }
    //
    // // Strategies
    // if (root["strategies"]) {
    //     for (const auto& strat_node : root["strategies"]) {
    //         AppConfig::StrategyConfig strat;
    //         strat.id = strat_node["id"].as<std::string>();
    //         strat.type = strat_node["type"].as<std::string>();
    //         strat.enabled = strat_node["enabled"].as<bool>(true);
    //         strat.mode = strat_node["mode"].as<std::string>("paper");
    //
    //         if (strat_node["instruments"]) {
    //             for (const auto& inst : strat_node["instruments"]) {
    //                 strat.instruments.push_back(inst.as<std::string>());
    //             }
    //         }
    //
    //         if (strat_node["parameters"]) {
    //             for (const auto& param : strat_node["parameters"]) {
    //                 strat.parameters[param.first.as<std::string>()] =
    //                     param.second.as<std::string>();
    //             }
    //         }
    //
    //         config.strategies.push_back(strat);
    //     }
    // }

    // Simplified parsing (for demonstration)
    auto kv = parse_simple_yaml(yaml_content);

    if (kv.count("environment")) {
        config.environment = kv["environment"];
    }

    // Database defaults
    config.database.host = kv.count("database_host") ? kv["database_host"] : "localhost";
    config.database.port = kv.count("database_port") ? std::stoi(kv["database_port"]) : 5432;
    config.database.database = kv.count("database_name") ? kv["database_name"] : "quantumliquidity";
    config.database.user = kv.count("database_user") ? kv["database_user"] : "quantumliquidity";
    config.database.password = kv.count("database_password") ? kv["database_password"] : "";

    // Redis defaults
    config.redis.host = kv.count("redis_host") ? kv["redis_host"] : "localhost";
    config.redis.port = kv.count("redis_port") ? std::stoi(kv["redis_port"]) : 6379;
    config.redis.db = kv.count("redis_db") ? std::stoi(kv["redis_db"]) : 0;

    // Risk defaults
    config.risk_limits.max_position_value_per_instrument = 100000.0;
    config.risk_limits.max_total_exposure = 500000.0;
    config.risk_limits.max_daily_loss = 10000.0;
    config.risk_limits.max_leverage = 10.0;

    // Logging defaults
    config.logging.level = kv.count("log_level") ? kv["log_level"] : "INFO";
    config.logging.console_enabled = true;
    config.logging.colored_console = true;

    Logger::info("system", "Configuration loaded successfully");

    return config;
}

void ConfigLoader::apply_env_overrides(AppConfig& config) {
    // Database overrides
    if (!get_env("DATABASE_HOST").empty()) {
        config.database.host = get_env("DATABASE_HOST");
    }
    if (!get_env("DATABASE_PORT").empty()) {
        config.database.port = std::stoi(get_env("DATABASE_PORT"));
    }
    if (!get_env("DATABASE_NAME").empty()) {
        config.database.database = get_env("DATABASE_NAME");
    }
    if (!get_env("DATABASE_USER").empty()) {
        config.database.user = get_env("DATABASE_USER");
    }
    if (!get_env("DATABASE_PASSWORD").empty()) {
        config.database.password = get_env("DATABASE_PASSWORD");
    }

    // Redis overrides
    if (!get_env("REDIS_HOST").empty()) {
        config.redis.host = get_env("REDIS_HOST");
    }
    if (!get_env("REDIS_PORT").empty()) {
        config.redis.port = std::stoi(get_env("REDIS_PORT"));
    }

    // Risk overrides
    if (!get_env("RISK_MAX_DAILY_LOSS").empty()) {
        config.risk_limits.max_daily_loss = std::stod(get_env("RISK_MAX_DAILY_LOSS"));
    }

    // Logging overrides
    if (!get_env("LOG_LEVEL").empty()) {
        config.logging.level = get_env("LOG_LEVEL");
    }

    // Environment override
    if (!get_env("ENVIRONMENT").empty()) {
        config.environment = get_env("ENVIRONMENT");
    }

    Logger::info("system", "Applied environment variable overrides");
}

bool ConfigLoader::validate(const AppConfig& config, std::string& error_message) {
    // Validate database config
    if (config.database.host.empty()) {
        error_message = "Database host cannot be empty";
        return false;
    }
    if (config.database.port <= 0 || config.database.port > 65535) {
        error_message = "Invalid database port: " + std::to_string(config.database.port);
        return false;
    }
    if (config.database.database.empty()) {
        error_message = "Database name cannot be empty";
        return false;
    }

    // Validate Redis config
    if (config.redis.host.empty()) {
        error_message = "Redis host cannot be empty";
        return false;
    }
    if (config.redis.port <= 0 || config.redis.port > 65535) {
        error_message = "Invalid Redis port: " + std::to_string(config.redis.port);
        return false;
    }

    // Validate risk limits
    if (config.risk_limits.max_daily_loss <= 0) {
        error_message = "Max daily loss must be positive";
        return false;
    }
    if (config.risk_limits.max_leverage <= 0) {
        error_message = "Max leverage must be positive";
        return false;
    }

    // Validate environment
    if (config.environment != "development" &&
        config.environment != "staging" &&
        config.environment != "production") {
        error_message = "Invalid environment: " + config.environment +
                       " (must be development, staging, or production)";
        return false;
    }

    Logger::info("system", "Configuration validation passed");
    return true;
}

} // namespace quantumliquidity::config
