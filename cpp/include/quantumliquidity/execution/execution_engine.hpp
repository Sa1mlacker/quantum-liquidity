#pragma once

#include "quantumliquidity/execution/types.hpp"
#include "quantumliquidity/execution/position_manager.hpp"
#include "quantumliquidity/execution/execution_provider.hpp"
#include "quantumliquidity/risk/risk_manager.hpp"
#include "quantumliquidity/common/redis_client.hpp"
#include <memory>
#include <mutex>
#include <map>
#include <functional>

namespace quantumliquidity::execution {

/**
 * @brief Execution Engine - central order routing and lifecycle management
 *
 * Thread-safe execution engine that:
 * - Validates orders via Risk Manager
 * - Routes orders to appropriate execution providers
 * - Processes fills and updates Position Manager
 * - Publishes all events to Redis for monitoring
 * - Tracks order lifecycle from submission to completion
 */
class ExecutionEngine {
public:
    /**
     * @brief Configuration for execution engine
     */
    struct Config {
        std::string redis_host = "localhost";
        int redis_port = 6379;
        std::string redis_password;
        bool enable_redis = true;
        int order_timeout_seconds = 30;
    };

    /**
     * @brief Order event callback
     */
    using OrderEventCallback = std::function<void(const OrderUpdate&)>;

    /**
     * @brief Fill event callback
     */
    using FillEventCallback = std::function<void(const Fill&)>;

    /**
     * @brief Constructor
     *
     * @param config Engine configuration
     * @param risk_mgr Risk manager (must outlive engine)
     * @param position_mgr Position manager (must outlive engine)
     */
    ExecutionEngine(
        const Config& config,
        risk::RiskManager* risk_mgr,
        PositionManager* position_mgr
    );

    ~ExecutionEngine();

    /**
     * @brief Register execution provider
     *
     * Providers handle actual order submission to brokers/exchanges.
     * Multiple providers can be registered (e.g., OANDA for FX, IB for stocks).
     *
     * @param name Provider name (e.g., "oanda", "interactive_brokers")
     * @param provider Execution provider instance
     */
    void register_provider(const std::string& name, std::shared_ptr<IExecutionProvider> provider);

    /**
     * @brief Set default provider for instrument routing
     *
     * @param instrument Instrument ID
     * @param provider_name Provider name to use for this instrument
     */
    void set_instrument_provider(const std::string& instrument, const std::string& provider_name);

    /**
     * @brief Submit order for execution
     *
     * Flow:
     * 1. Risk check via RiskManager
     * 2. Route to appropriate provider
     * 3. Publish order event to Redis
     * 4. Return immediate result
     *
     * @param order Order request
     * @return Order update with initial status
     */
    OrderUpdate submit_order(const OrderRequest& order);

    /**
     * @brief Cancel pending order
     *
     * @param order_id Order to cancel
     * @return Order update with cancellation status
     */
    OrderUpdate cancel_order(const std::string& order_id);

    /**
     * @brief Modify pending order
     *
     * @param modification Modification request
     * @return Order update with modification status
     */
    OrderUpdate modify_order(const OrderModification& modification);

    /**
     * @brief Process fill from provider
     *
     * Called by execution providers when fills occur.
     * Updates position manager and publishes to Redis.
     *
     * @param fill Executed fill
     */
    void on_fill(const Fill& fill);

    /**
     * @brief Process order update from provider
     *
     * Called by providers when order status changes.
     *
     * @param update Order update
     */
    void on_order_update(const OrderUpdate& update);

    /**
     * @brief Get current order status
     *
     * @param order_id Order ID
     * @return Current order update, or nullopt if not found
     */
    std::optional<OrderUpdate> get_order_status(const std::string& order_id) const;

    /**
     * @brief Get all active orders
     *
     * @return Map of order_id to current status
     */
    std::map<std::string, OrderUpdate> get_active_orders() const;

    /**
     * @brief Register callback for order events
     *
     * @param callback Function to call on order updates
     */
    void register_order_callback(OrderEventCallback callback);

    /**
     * @brief Register callback for fill events
     *
     * @param callback Function to call on fills
     */
    void register_fill_callback(FillEventCallback callback);

    /**
     * @brief Get execution statistics
     */
    struct Stats {
        int total_orders_submitted;
        int total_orders_filled;
        int total_orders_rejected;
        int total_orders_cancelled;
        int active_orders;
        double total_volume_traded;
        int64_t last_fill_timestamp_ns;
    };

    Stats get_stats() const;

    /**
     * @brief Shutdown engine gracefully
     *
     * Cancels all pending orders and closes connections.
     */
    void shutdown();

private:
    // Internal order state tracking
    struct OrderState {
        OrderRequest request;
        OrderUpdate current_status;
        std::string provider_name;
        int64_t submit_timestamp_ns;
        int64_t last_update_ns;
    };

    // Helper: route order to appropriate provider
    std::string select_provider_for_order(const OrderRequest& order) const;

    // Helper: publish order event to Redis
    void publish_order_event(const OrderUpdate& update);

    // Helper: publish fill event to Redis
    void publish_fill_event(const Fill& fill);

    // Helper: update order state
    void update_order_state(const OrderUpdate& update);

    // Helper: remove order from active tracking
    void finalize_order(const std::string& order_id);

    Config config_;
    risk::RiskManager* risk_mgr_;
    PositionManager* position_mgr_;

    // Execution providers
    std::map<std::string, std::shared_ptr<IExecutionProvider>> providers_;
    std::map<std::string, std::string> instrument_routing_;  // instrument -> provider_name
    std::string default_provider_;

    // Order tracking
    std::map<std::string, OrderState> active_orders_;
    std::map<std::string, OrderState> completed_orders_;  // Recent history

    // Callbacks
    std::vector<OrderEventCallback> order_callbacks_;
    std::vector<FillEventCallback> fill_callbacks_;

    // Redis connection
    std::unique_ptr<common::RedisClient> redis_;

    // Statistics
    Stats stats_;

    mutable std::mutex mutex_;
    bool shutdown_requested_;
};

} // namespace quantumliquidity::execution
