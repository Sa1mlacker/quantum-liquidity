#pragma once

#include "quantumliquidity/execution/execution_provider.hpp"
#include "quantumliquidity/execution/execution_engine.hpp"
#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <random>

namespace quantumliquidity::execution {

/**
 * @brief Mock Broker Provider for testing
 *
 * Simulates a broker/exchange without real network connections.
 * Features:
 * - Configurable fill latency
 * - Partial fills support
 * - Configurable rejection rate
 * - Realistic order lifecycle simulation
 * - Market/Limit order handling
 */
class MockBroker : public IExecutionProvider {
public:
    /**
     * @brief Configuration for mock broker behavior
     */
    struct Config {
        std::string broker_name = "MockBroker";
        int fill_latency_ms = 100;          // Simulated fill delay
        double rejection_rate = 0.0;        // 0.0 = never reject, 1.0 = always reject
        bool enable_partial_fills = false;  // Split orders into multiple fills
        int partial_fill_count = 3;         // How many fills per order
        double slippage_bps = 0.0;          // Slippage in basis points (0.01 = 1bp)
        bool auto_connect = true;           // Connect on construction
    };

    explicit MockBroker(const Config& config = Config());
    ~MockBroker() override;

    // IExecutionProvider interface
    OrderUpdate submit_order(const OrderRequest& order) override;
    OrderUpdate cancel_order(const std::string& order_id) override;
    OrderUpdate modify_order(const OrderModification& modification) override;
    std::optional<OrderUpdate> get_order_status(const std::string& order_id) override;
    void set_execution_engine(ExecutionEngine* engine) override;
    bool connect() override;
    void disconnect() override;
    bool is_connected() const override;
    std::string get_name() const override;

    /**
     * @brief Set market price for instrument
     *
     * Used for realistic order simulation.
     *
     * @param instrument Instrument ID
     * @param price Current market price
     */
    void set_market_price(const std::string& instrument, double price);

    /**
     * @brief Get statistics for testing
     */
    struct Stats {
        int orders_received = 0;
        int orders_filled = 0;
        int orders_rejected = 0;
        int orders_cancelled = 0;
        int fills_generated = 0;
    };

    Stats get_stats() const;

private:
    // Internal order state
    struct OrderState {
        OrderRequest request;
        OrderUpdate current_status;
        double filled_qty;
        double remaining_qty;
        int64_t submit_timestamp_ns;
        bool cancelled;
    };

    // Helper: simulate order fill asynchronously
    void simulate_fill(const std::string& order_id);

    // Helper: generate fill ID
    std::string generate_fill_id();

    // Helper: calculate fill price with slippage
    double calculate_fill_price(const OrderRequest& order, double market_price);

    // Helper: should this order be rejected?
    bool should_reject();

    Config config_;
    ExecutionEngine* engine_;
    bool connected_;

    // Order tracking
    std::map<std::string, OrderState> orders_;
    mutable std::mutex mutex_;

    // Market prices for simulation
    std::map<std::string, double> market_prices_;

    // Stats
    Stats stats_;

    // RNG for simulation
    std::mt19937 rng_;
    std::uniform_real_distribution<double> uniform_dist_;

    // Fill simulation
    std::atomic<bool> shutdown_requested_;
    std::vector<std::thread> fill_threads_;
    int next_fill_id_;
};

} // namespace quantumliquidity::execution
