#pragma once

#include "quantumliquidity/common/types.hpp"
#include <memory>
#include <vector>

namespace quantumliquidity::execution {

/**
 * @brief Order modification request
 */
struct OrderModification {
    OrderID order_id;
    std::optional<Quantity> new_quantity;
    std::optional<Price> new_limit_price;
    std::optional<Price> new_stop_price;
};

/**
 * @brief Abstract interface for order submission
 */
class IOrderSender {
public:
    virtual ~IOrderSender() = default;

    virtual OrderID submit_order(const OrderRequest& request) = 0;
    virtual void cancel_order(OrderID id) = 0;
    virtual void modify_order(const OrderModification& modification) = 0;

    // Query order state
    virtual std::optional<OrderUpdate> get_order_status(OrderID id) const = 0;
    virtual std::vector<OrderUpdate> get_active_orders() const = 0;
};

/**
 * @brief Execution provider interface (broker-specific)
 *
 * Implements actual communication with broker API.
 */
class IExecutionProvider {
public:
    virtual ~IExecutionProvider() = default;

    // Connection
    virtual void connect() = 0;
    virtual void disconnect() = 0;
    virtual bool is_connected() const = 0;

    // Order operations
    virtual OrderID send_order(const OrderRequest& request) = 0;
    virtual void cancel_order(OrderID id) = 0;
    virtual void replace_order(const OrderModification& modification) = 0;

    // Callbacks for order updates
    using OrderUpdateCallback = std::function<void(const OrderUpdate&)>;
    using FillCallback = std::function<void(const Fill&)>;

    virtual void set_order_update_callback(OrderUpdateCallback callback) = 0;
    virtual void set_fill_callback(FillCallback callback) = 0;

    // Metadata
    virtual std::string provider_name() const = 0;
};

/**
 * @brief Execution engine (combines provider + risk checks)
 *
 * This is the main entry point for strategies to submit orders.
 * It routes through risk engine before hitting the provider.
 */
class IExecutionEngine : public IOrderSender {
public:
    virtual ~IExecutionEngine() = default;

    // Start/stop
    virtual void start() = 0;
    virtual void stop() = 0;

    // Provider management
    virtual void set_provider(std::shared_ptr<IExecutionProvider> provider) = 0;

    // Statistics
    struct Stats {
        uint64_t orders_submitted;
        uint64_t orders_filled;
        uint64_t orders_cancelled;
        uint64_t orders_rejected;
        uint64_t risk_checks_failed;
    };

    virtual Stats get_stats() const = 0;
};

} // namespace quantumliquidity::execution
