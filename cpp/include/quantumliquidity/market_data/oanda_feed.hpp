#pragma once

#include "quantumliquidity/market_data/feed_interface.hpp"
#include <string>
#include <thread>
#include <atomic>
#include <curl/curl.h>

namespace quantumliquidity::market_data {

/**
 * @brief OANDA v20 REST API + Streaming feed
 *
 * Provides live tick data from OANDA via their v20 API.
 * Supports both practice (demo) and live accounts.
 *
 * API Documentation: https://developer.oanda.com/rest-live-v20/introduction/
 */
class OANDAFeed : public IMarketDataFeed {
public:
    struct Config {
        std::string api_token;              // Your OANDA API token
        std::string account_id;             // Your OANDA account ID
        bool use_practice = true;           // true = demo, false = live
        std::string feed_name = "OANDA";
        
        // API endpoints
        std::string rest_url = "https://api-fxpractice.oanda.com";    // Practice
        std::string stream_url = "https://stream-fxpractice.oanda.com";  // Practice streaming
        
        // For live trading, use:
        // rest_url = "https://api-fxtrade.oanda.com"
        // stream_url = "https://stream-fxtrade.oanda.com"
    };

    explicit OANDAFeed(Config config);
    ~OANDAFeed() override;

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

private:
    void streaming_thread_func();
    void reconnect_stream();
    
    // HTTP helpers
    std::string http_get(const std::string& endpoint);
    std::string http_stream(const std::string& endpoint);
    
    // Parsing
    Tick parse_pricing_stream(const std::string& json_line);
    std::string convert_instrument_name(const InstrumentID& instrument) const;
    
    Config config_;
    std::atomic<bool> connected_;
    std::atomic<bool> running_;
    std::atomic<bool> should_reconnect_;

    std::thread streaming_thread_;
    CURL* curl_handle_;
    CURL* stream_handle_;

    mutable std::mutex mutex_;
    std::set<InstrumentID> subscribed_instruments_;

    TickCallback tick_callback_;
    BarCallback bar_callback_;
    DepthCallback depth_callback_;
    ErrorCallback error_callback_;

    uint64_t ticks_received_;
    int reconnect_attempts_;
};

} // namespace quantumliquidity::market_data
