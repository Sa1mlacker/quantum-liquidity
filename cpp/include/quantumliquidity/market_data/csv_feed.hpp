#pragma once

#include "quantumliquidity/market_data/feed_interface.hpp"
#include <string>
#include <thread>
#include <atomic>
#include <fstream>
#include <queue>

namespace quantumliquidity::market_data {

/**
 * @brief CSV file-based market data feed for testing and backtesting
 *
 * Reads tick data from CSV files and replays them as if live.
 * Useful for testing without broker connection.
 *
 * CSV Format (required columns):
 * timestamp,instrument,bid,ask,bid_size,ask_size
 *
 * Example:
 * 2024-01-01 09:30:00.000,EUR/USD,1.10245,1.10250,1000000,1000000
 */
class CSVFeed : public IMarketDataFeed {
public:
    struct Config {
        std::string csv_filepath;
        std::string feed_name = "CSV";
        double replay_speed = 1.0;  // 1.0 = real-time, 10.0 = 10x faster, 0 = no delay
        bool loop = false;          // Restart from beginning when file ends
    };

    explicit CSVFeed(Config config);
    ~CSVFeed() override;

    // IMarketDataFeed implementation
    void subscribe_ticks(const InstrumentID& instrument) override;
    void subscribe_bars(const InstrumentID& instrument, TimeFrame tf) override;
    void subscribe_depth(const InstrumentID& instrument, int levels) override;

    void unsubscribe_ticks(const InstrumentID& instrument) override;
    void unsubscribe_bars(const InstrumentID& instrument, TimeFrame tf) override;
    void unsubscribe_depth(const InstrumentID& instrument) override;

    void connect() override;
    void disconnect() override;
    bool is_connected() const override;

    void set_tick_callback(TickCallback callback) override;
    void set_bar_callback(BarCallback callback) override;
    void set_depth_callback(DepthCallback callback) override;
    void set_error_callback(ErrorCallback callback) override;

    std::string name() const override;
    std::vector<InstrumentInfo> available_instruments() const override;

    // CSV-specific methods
    void set_replay_speed(double multiplier);
    void pause();
    void resume();
    bool is_paused() const;

private:
    void replay_thread_func();
    Tick parse_csv_line(const std::string& line);
    bool is_subscribed(const InstrumentID& instrument) const;

    Config config_;
    std::atomic<bool> connected_;
    std::atomic<bool> running_;
    std::atomic<bool> paused_;

    std::thread replay_thread_;
    std::ifstream csv_file_;

    mutable std::mutex mutex_;
    std::set<InstrumentID> subscribed_instruments_;

    TickCallback tick_callback_;
    BarCallback bar_callback_;
    DepthCallback depth_callback_;
    ErrorCallback error_callback_;

    Timestamp last_tick_time_;
    uint64_t ticks_replayed_;
};

} // namespace quantumliquidity::market_data
