#pragma once

#include <string>
#include <memory>
#include <map>

namespace quantumliquidity {

/**
 * @brief Structured logger with channel-based routing
 *
 * Channels:
 * - market_data: tick/bar reception, feed status
 * - orders: order lifecycle (submit, accept, fill, cancel)
 * - fills: execution events with details
 * - risk: risk checks, limit breaches, kill-switch
 * - strategies: strategy signals, entry/exit decisions
 * - database: database operations
 * - redis: redis pub/sub events
 * - system: system events, startup/shutdown
 * - errors: all error-level messages (cross-channel)
 *
 * Each channel can have:
 * - Separate log file
 * - Different log level
 * - Console output (optional)
 */
class Logger {
public:
    enum class Level {
        TRACE = 0,
        DEBUG = 1,
        INFO = 2,
        WARNING = 3,
        ERROR = 4,
        CRITICAL = 5
    };

    // Initialize logging system
    static void initialize();
    static void shutdown();

    // Main logging method
    static void log(Level level, const std::string& channel, const std::string& message);

    // Convenience methods
    static void trace(const std::string& channel, const std::string& message);
    static void debug(const std::string& channel, const std::string& message);
    static void info(const std::string& channel, const std::string& message);
    static void warning(const std::string& channel, const std::string& message);
    static void error(const std::string& channel, const std::string& message);
    static void critical(const std::string& channel, const std::string& message);

    // Configuration
    static void set_global_level(Level level);
    static void set_channel_level(const std::string& channel, Level level);

    // Add sinks (outputs)
    static void add_console_sink(bool colored = true);
    static void add_file_sink(
        const std::string& channel,
        const std::string& filepath,
        bool rotate = true,
        size_t max_size_mb = 10,
        size_t max_files = 5
    );
    static void add_global_file_sink(const std::string& filepath);

    // Flush logs (call before exit)
    static void flush();

private:
    Logger() = default;
    static Logger& instance();

    class Impl;
    std::unique_ptr<Impl> impl_;
    static std::unique_ptr<Logger> instance_;
};

// Macros for convenient logging with source location
#define LOG_TRACE(channel, msg) \
    quantumliquidity::Logger::trace(channel, msg)

#define LOG_DEBUG(channel, msg) \
    quantumliquidity::Logger::debug(channel, msg)

#define LOG_INFO(channel, msg) \
    quantumliquidity::Logger::info(channel, msg)

#define LOG_WARNING(channel, msg) \
    quantumliquidity::Logger::warning(channel, msg)

#define LOG_ERROR(channel, msg) \
    quantumliquidity::Logger::error(channel, msg)

#define LOG_CRITICAL(channel, msg) \
    quantumliquidity::Logger::critical(channel, msg)

} // namespace quantumliquidity
