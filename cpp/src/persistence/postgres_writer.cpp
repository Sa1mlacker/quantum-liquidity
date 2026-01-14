#include "quantumliquidity/persistence/database.hpp"
#include "quantumliquidity/common/logger.hpp"
#include "quantumliquidity/common/utils.hpp"
#include <queue>
#include <mutex>
#include <thread>
#include <chrono>
#include <sstream>
#include <map>

namespace quantumliquidity::persistence {

/**
 * @brief Batch writer implementation with periodic flushing
 */
class TimeSeriesWriter : public ITimeSeriesWriter {
public:
    explicit TimeSeriesWriter(
        std::shared_ptr<IConnectionPool> pool,
        int batch_size = 1000,
        int flush_interval_ms = 1000
    )
        : pool_(std::move(pool))
        , batch_size_(batch_size)
        , flush_interval_ms_(flush_interval_ms)
        , running_(false)
        , stats_{0, 0, 0, 0, 0, 0}
    {}

    ~TimeSeriesWriter() override {
        stop();
    }

    void start() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (running_) {
            return;
        }

        running_ = true;

        // Start background flush thread
        flush_thread_ = std::thread([this] { flush_loop(); });

        Logger::info("database", "TimeSeriesWriter started");
    }

    void stop() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (!running_) {
                return;
            }
            running_ = false;
        }

        cv_.notify_one();

        if (flush_thread_.joinable()) {
            flush_thread_.join();
        }

        // Final flush
        flush();

        Logger::info("database", "TimeSeriesWriter stopped");
    }

    void write_tick(const Tick& tick) override {
        std::lock_guard<std::mutex> lock(mutex_);
        tick_buffer_.push_back(tick);

        if (tick_buffer_.size() >= static_cast<size_t>(batch_size_)) {
            flush_ticks_unlocked();
        }
    }

    void write_ticks(const std::vector<Tick>& ticks) override {
        std::lock_guard<std::mutex> lock(mutex_);
        tick_buffer_.insert(tick_buffer_.end(), ticks.begin(), ticks.end());

        if (tick_buffer_.size() >= static_cast<size_t>(batch_size_)) {
            flush_ticks_unlocked();
        }
    }

    void write_bar(const Bar& bar) override {
        std::lock_guard<std::mutex> lock(mutex_);
        bar_buffer_.push_back(bar);

        if (bar_buffer_.size() >= static_cast<size_t>(batch_size_)) {
            flush_bars_unlocked();
        }
    }

    void write_bars(const std::vector<Bar>& bars) override {
        std::lock_guard<std::mutex> lock(mutex_);
        bar_buffer_.insert(bar_buffer_.end(), bars.begin(), bars.end());

        if (bar_buffer_.size() >= static_cast<size_t>(batch_size_)) {
            flush_bars_unlocked();
        }
    }

    void write_order(const OrderUpdate& order) override {
        std::lock_guard<std::mutex> lock(mutex_);
        order_buffer_.push_back(order);

        if (order_buffer_.size() >= static_cast<size_t>(batch_size_)) {
            flush_orders_unlocked();
        }
    }

    void write_fill(const Fill& fill) override {
        std::lock_guard<std::mutex> lock(mutex_);
        fill_buffer_.push_back(fill);

        if (fill_buffer_.size() >= static_cast<size_t>(batch_size_)) {
            flush_fills_unlocked();
        }
    }

    void flush() override {
        std::lock_guard<std::mutex> lock(mutex_);
        flush_all_unlocked();
    }

    Stats get_stats() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_;
    }

private:
    void flush_loop() {
        while (running_) {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait_for(lock, std::chrono::milliseconds(flush_interval_ms_),
                [this] { return !running_; });

            if (!running_) {
                break;
            }

            flush_all_unlocked();
        }
    }

    void flush_all_unlocked() {
        flush_ticks_unlocked();
        flush_bars_unlocked();
        flush_orders_unlocked();
        flush_fills_unlocked();
        stats_.flush_count++;
    }

    void flush_ticks_unlocked() {
        if (tick_buffer_.empty()) {
            return;
        }

        try {
            auto conn = pool_->acquire();

            // Build batch INSERT
            std::ostringstream query;
            query << "INSERT INTO ticks (timestamp, instrument, bid, ask, "
                  << "bid_size, ask_size, last_trade_price, last_trade_size) VALUES ";

            for (size_t i = 0; i < tick_buffer_.size(); i++) {
                const auto& tick = tick_buffer_[i];

                if (i > 0) query << ", ";

                query << "('"
                      << utils::timestamp_to_string(tick.timestamp) << "', '"
                      << tick.instrument << "', "
                      << tick.bid << ", "
                      << tick.ask << ", "
                      << tick.bid_size << ", "
                      << tick.ask_size << ", "
                      << (tick.last_trade_price.has_value() ?
                          std::to_string(*tick.last_trade_price) : "NULL") << ", "
                      << (tick.last_trade_size.has_value() ?
                          std::to_string(*tick.last_trade_size) : "NULL")
                      << ")";
            }

            query << " ON CONFLICT (timestamp, instrument) DO NOTHING";

            conn->execute(query.str());
            pool_->release(conn);

            stats_.ticks_written += tick_buffer_.size();
            tick_buffer_.clear();

            Logger::debug("database", "Flushed ticks");

        } catch (const std::exception& e) {
            stats_.error_count++;
            Logger::error("database", "Failed to flush ticks: " + std::string(e.what()));
        }
    }

    void flush_bars_unlocked() {
        if (bar_buffer_.empty()) {
            return;
        }

        try {
            // Group bars by timeframe
            std::map<TimeFrame, std::vector<Bar>> bars_by_tf;
            for (const auto& bar : bar_buffer_) {
                bars_by_tf[bar.timeframe].push_back(bar);
            }

            auto conn = pool_->acquire();

            for (const auto& [tf, bars] : bars_by_tf) {
                std::string table_name = get_bar_table_name(tf);

                std::ostringstream query;
                query << "INSERT INTO " << table_name
                      << " (timestamp, instrument, open, high, low, close, volume, tick_count) VALUES ";

                for (size_t i = 0; i < bars.size(); i++) {
                    const auto& bar = bars[i];

                    if (i > 0) query << ", ";

                    query << "('"
                          << utils::timestamp_to_string(bar.timestamp) << "', '"
                          << bar.instrument << "', "
                          << bar.open << ", "
                          << bar.high << ", "
                          << bar.low << ", "
                          << bar.close << ", "
                          << bar.volume << ", "
                          << bar.tick_count
                          << ")";
                }

                query << " ON CONFLICT (timestamp, instrument) DO UPDATE SET "
                      << "open=EXCLUDED.open, high=EXCLUDED.high, low=EXCLUDED.low, "
                      << "close=EXCLUDED.close, volume=EXCLUDED.volume, tick_count=EXCLUDED.tick_count";

                conn->execute(query.str());
                stats_.bars_written += bars.size();
            }

            pool_->release(conn);
            bar_buffer_.clear();

            Logger::debug("database", "Flushed bars");

        } catch (const std::exception& e) {
            stats_.error_count++;
            Logger::error("database", "Failed to flush bars: " + std::string(e.what()));
        }
    }

    void flush_orders_unlocked() {
        if (order_buffer_.empty()) {
            return;
        }

        // TODO: Implement order flushing similar to ticks
        stats_.orders_written += order_buffer_.size();
        order_buffer_.clear();
    }

    void flush_fills_unlocked() {
        if (fill_buffer_.empty()) {
            return;
        }

        // TODO: Implement fills flushing similar to ticks
        stats_.fills_written += fill_buffer_.size();
        fill_buffer_.clear();
    }

    std::string get_bar_table_name(TimeFrame tf) const {
        switch (tf) {
            case TimeFrame::MIN_1: return "bars_1m";
            case TimeFrame::MIN_5: return "bars_5m";
            case TimeFrame::MIN_15: return "bars_15m";
            case TimeFrame::MIN_30: return "bars_30m";
            case TimeFrame::HOUR_1: return "bars_1h";
            case TimeFrame::HOUR_4: return "bars_4h";
            case TimeFrame::DAY_1: return "bars_1d";
            default: return "bars_1m";
        }
    }

    std::shared_ptr<IConnectionPool> pool_;
    int batch_size_;
    int flush_interval_ms_;
    bool running_;

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::thread flush_thread_;

    std::vector<Tick> tick_buffer_;
    std::vector<Bar> bar_buffer_;
    std::vector<OrderUpdate> order_buffer_;
    std::vector<Fill> fill_buffer_;

    Stats stats_;
};

// Factory
std::unique_ptr<ITimeSeriesWriter> create_time_series_writer(
    std::shared_ptr<IConnectionPool> pool,
    int batch_size,
    int flush_interval_ms
) {
    auto writer = std::make_unique<TimeSeriesWriter>(pool, batch_size, flush_interval_ms);
    return writer;
}

} // namespace quantumliquidity::persistence
