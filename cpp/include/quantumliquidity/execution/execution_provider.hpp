#pragma once

#include "quantumliquidity/execution/types.hpp"
#include <functional>
#include <string>

namespace quantumliquidity::execution {

// Forward declaration
class ExecutionEngine;

/**
 * @brief Execution Provider Interface
 *
 * Abstract interface for order execution backends.
 * Implementations connect to brokers/exchanges to execute trades.
 *
 * Thread-safety: Implementations must be thread-safe.
 */
class IExecutionProvider {
public:
    virtual ~IExecutionProvider() = default;

    /**
     * @brief Submit order to broker/exchange
     *
     * @param order Order request
     * @return Initial order status (usually SUBMITTED or REJECTED)
     */
    virtual OrderUpdate submit_order(const OrderRequest& order) = 0;

    /**
     * @brief Cancel pending order
     *
     * @param order_id Order to cancel
     * @return Updated order status
     */
    virtual OrderUpdate cancel_order(const std::string& order_id) = 0;

    /**
     * @brief Modify pending order
     *
     * @param modification Modification request
     * @return Updated order status
     */
    virtual OrderUpdate modify_order(const OrderModification& modification) = 0;

    /**
     * @brief Get current order status from broker
     *
     * @param order_id Order to query
     * @return Current status, or nullopt if not found
     */
    virtual std::optional<OrderUpdate> get_order_status(const std::string& order_id) = 0;

    /**
     * @brief Set execution engine for callbacks
     *
     * Provider calls engine->on_fill() and engine->on_order_update()
     * when events occur.
     *
     * @param engine Execution engine (must outlive provider)
     */
    virtual void set_execution_engine(ExecutionEngine* engine) = 0;

    /**
     * @brief Connect to broker/exchange
     *
     * @return true if connected successfully
     */
    virtual bool connect() = 0;

    /**
     * @brief Disconnect from broker/exchange
     */
    virtual void disconnect() = 0;

    /**
     * @brief Check if provider is connected
     *
     * @return true if connected and ready to accept orders
     */
    virtual bool is_connected() const = 0;

    /**
     * @brief Get provider name
     *
     * @return Human-readable provider name (e.g., "OANDA", "Interactive Brokers")
     */
    virtual std::string get_name() const = 0;
};

} // namespace quantumliquidity::execution
