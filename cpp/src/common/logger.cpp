#include "quantumliquidity/common/logger.hpp"
#include <iostream>
#include <fstream>
#include <mutex>
#include <ctime>
#include <iomanip>
#include <sstream>

// TODO: When spdlog is available, replace with:
// #include <spdlog/spdlog.h>
// #include <spdlog/sinks/stdout_color_sinks.h>
// #include <spdlog/sinks/rotating_file_sink.h>
// #include <spdlog/sinks/daily_file_sink.h>

namespace quantumliquidity {

/**
 * @brief Logger implementation (simplified, production should use spdlog)
 */
class Logger::Impl {
public:
    Impl() : global_level_(Level::INFO), console_enabled_(false) {}

    void log(Level level, const std::string& channel, const std::string& message) {
        std::lock_guard<std::mutex> lock(mutex_);

        // Check level threshold
        Level channel_level = get_channel_level(channel);
        if (level < channel_level) {
            return;
        }

        // Format message
        std::string formatted = format_message(level, channel, message);

        // Output to console
        if (console_enabled_) {
            std::cout << formatted << std::endl;
        }

        // Output to channel file
        auto it = channel_files_.find(channel);
        if (it != channel_files_.end() && it->second.is_open()) {
            it->second << formatted << std::endl;
            it->second.flush();
        }

        // Output to global file
        if (global_file_.is_open()) {
            global_file_ << formatted << std::endl;
            global_file_.flush();
        }

        // Always write errors to error file
        if (level >= Level::ERROR) {
            if (error_file_.is_open()) {
                error_file_ << formatted << std::endl;
                error_file_.flush();
            }
        }
    }

    void set_global_level(Level level) {
        std::lock_guard<std::mutex> lock(mutex_);
        global_level_ = level;
    }

    void set_channel_level(const std::string& channel, Level level) {
        std::lock_guard<std::mutex> lock(mutex_);
        channel_levels_[channel] = level;
    }

    void add_console_sink(bool colored) {
        std::lock_guard<std::mutex> lock(mutex_);
        console_enabled_ = true;
        // TODO: With spdlog, add colored console sink
    }

    void add_file_sink(
        const std::string& channel,
        const std::string& filepath,
        bool rotate,
        size_t max_size_mb,
        size_t max_files
    ) {
        std::lock_guard<std::mutex> lock(mutex_);

        // Close existing file if open
        if (channel_files_.count(channel)) {
            channel_files_[channel].close();
        }

        // Open new file
        channel_files_[channel].open(filepath, std::ios::app);

        if (!channel_files_[channel].is_open()) {
            std::cerr << "Failed to open log file: " << filepath << std::endl;
        }

        // TODO: With spdlog, use rotating_file_sink with max_size_mb and max_files
    }

    void add_global_file_sink(const std::string& filepath) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (global_file_.is_open()) {
            global_file_.close();
        }

        global_file_.open(filepath, std::ios::app);

        if (!global_file_.is_open()) {
            std::cerr << "Failed to open global log file: " << filepath << std::endl;
        }

        // Open error file
        std::string error_filepath = filepath;
        size_t pos = error_filepath.find_last_of('.');
        if (pos != std::string::npos) {
            error_filepath.insert(pos, "_errors");
        } else {
            error_filepath += "_errors";
        }

        error_file_.open(error_filepath, std::ios::app);
    }

    void flush() {
        std::lock_guard<std::mutex> lock(mutex_);

        for (auto& [channel, file] : channel_files_) {
            if (file.is_open()) {
                file.flush();
            }
        }

        if (global_file_.is_open()) {
            global_file_.flush();
        }

        if (error_file_.is_open()) {
            error_file_.flush();
        }
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(mutex_);

        for (auto& [channel, file] : channel_files_) {
            if (file.is_open()) {
                file.close();
            }
        }

        if (global_file_.is_open()) {
            global_file_.close();
        }

        if (error_file_.is_open()) {
            error_file_.close();
        }
    }

private:
    Level get_channel_level(const std::string& channel) const {
        auto it = channel_levels_.find(channel);
        if (it != channel_levels_.end()) {
            return it->second;
        }
        return global_level_;
    }

    std::string format_message(Level level, const std::string& channel, const std::string& message) const {
        std::ostringstream oss;

        // Timestamp
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()
        ) % 1000;

        oss << "[" << std::put_time(std::localtime(&time_t_now), "%Y-%m-%d %H:%M:%S")
            << "." << std::setfill('0') << std::setw(3) << ms.count() << "] ";

        // Level
        oss << "[" << level_to_string(level) << "] ";

        // Channel
        oss << "[" << channel << "] ";

        // Message
        oss << message;

        return oss.str();
    }

    std::string level_to_string(Level level) const {
        switch (level) {
            case Level::TRACE: return "TRACE";
            case Level::DEBUG: return "DEBUG";
            case Level::INFO: return "INFO ";
            case Level::WARNING: return "WARN ";
            case Level::ERROR: return "ERROR";
            case Level::CRITICAL: return "CRIT ";
            default: return "UNKN ";
        }
    }

    std::mutex mutex_;
    Level global_level_;
    std::map<std::string, Level> channel_levels_;
    bool console_enabled_;

    std::map<std::string, std::ofstream> channel_files_;
    std::ofstream global_file_;
    std::ofstream error_file_;
};

// Static instance
std::unique_ptr<Logger> Logger::instance_ = nullptr;

Logger& Logger::instance() {
    if (!instance_) {
        instance_ = std::unique_ptr<Logger>(new Logger());
        instance_->impl_ = std::make_unique<Impl>();
    }
    return *instance_;
}

void Logger::initialize() {
    instance();  // Ensure instance exists
}

void Logger::shutdown() {
    if (instance_) {
        instance_->impl_->shutdown();
        instance_.reset();
    }
}

void Logger::log(Level level, const std::string& channel, const std::string& message) {
    instance().impl_->log(level, channel, message);
}

void Logger::trace(const std::string& channel, const std::string& message) {
    log(Level::TRACE, channel, message);
}

void Logger::debug(const std::string& channel, const std::string& message) {
    log(Level::DEBUG, channel, message);
}

void Logger::info(const std::string& channel, const std::string& message) {
    log(Level::INFO, channel, message);
}

void Logger::warning(const std::string& channel, const std::string& message) {
    log(Level::WARNING, channel, message);
}

void Logger::error(const std::string& channel, const std::string& message) {
    log(Level::ERROR, channel, message);
}

void Logger::critical(const std::string& channel, const std::string& message) {
    log(Level::CRITICAL, channel, message);
}

void Logger::set_global_level(Level level) {
    instance().impl_->set_global_level(level);
}

void Logger::set_channel_level(const std::string& channel, Level level) {
    instance().impl_->set_channel_level(channel, level);
}

void Logger::add_console_sink(bool colored) {
    instance().impl_->add_console_sink(colored);
}

void Logger::add_file_sink(
    const std::string& channel,
    const std::string& filepath,
    bool rotate,
    size_t max_size_mb,
    size_t max_files
) {
    instance().impl_->add_file_sink(channel, filepath, rotate, max_size_mb, max_files);
}

void Logger::add_global_file_sink(const std::string& filepath) {
    instance().impl_->add_global_file_sink(filepath);
}

void Logger::flush() {
    if (instance_) {
        instance_->impl_->flush();
    }
}

} // namespace quantumliquidity
