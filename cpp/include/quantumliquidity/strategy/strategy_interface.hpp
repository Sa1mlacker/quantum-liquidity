#pragma once

#include "quantumliquidity/common/types.hpp"
#include <memory>
#include <string>

namespace quantumliquidity::strategy {

/**
 * @brief Analytics event from Python analytics engine
 */
struct AnalyticsEvent {
    enum class Type {
        DAY_TYPE_CLASSIFIED,
        ORB_BREAKOUT,
        SESSION_TRANSITION,
        VOLATILITY_SPIKE,
        VOLUME_PROFILE_UPDATE
    };

    Type type;
    Timestamp timestamp;
    InstrumentID instrument;
    std::string data;  // JSON payload with event-specific data
};

/**
 * @brief Core strategy interface
 *
 * All strategies (C++ or Python-wrapped) implement this interface.
 */
class IStrategy {
public:
    virtual ~IStrategy() = default;

    // Lifecycle
    virtual void on_start() = 0;
    virtual void on_stop() = 0;

    // Market data events
    virtual void on_tick(const Tick& tick) = 0;
    virtual void on_bar(const Bar& bar) = 0;
    virtual void on_depth_update(const DepthUpdate& update) = 0;

    // Analytics/sentiment events
    virtual void on_analytics_event(const AnalyticsEvent& event) = 0;

    // Order feedback
    virtual void on_order_update(const OrderUpdate& update) = 0;
    virtual void on_fill(const Fill& fill) = 0;

    // Position update (from risk engine)
    virtual void on_position_update(const Position& position) = 0;

    // Metadata
    virtual std::string name() const = 0;
    virtual StrategyState state() const = 0;
    virtual std::vector<InstrumentID> instruments() const = 0;
};

/**
 * @brief Strategy context (injected dependencies)
 *
 * Provides access to order submission, market data queries, etc.
 */
class IStrategyContext {
public:
    virtual ~IStrategyContext() = default;

    // Order management
    virtual OrderID submit_order(const OrderRequest& request) = 0;
    virtual void cancel_order(OrderID id) = 0;

    // Position query
    virtual Position get_position(const InstrumentID& instrument) const = 0;

    // Market data query (latest known)
    virtual std::optional<Tick> get_last_tick(const InstrumentID& instrument) const = 0;
    virtual std::optional<Bar> get_last_bar(const InstrumentID& instrument, TimeFrame tf) const = 0;

    // Logging
    virtual void log_info(const std::string& message) = 0;
    virtual void log_warning(const std::string& message) = 0;
    virtual void log_error(const std::string& message) = 0;
};

/**
 * @brief Base strategy class with context injection
 */
class BaseStrategy : public IStrategy {
public:
    explicit BaseStrategy(std::shared_ptr<IStrategyContext> context)
        : context_(std::move(context))
        , state_(StrategyState::INACTIVE)
    {}

    StrategyState state() const override { return state_; }

protected:
    std::shared_ptr<IStrategyContext> context_;
    StrategyState state_;

    // Helper: submit order with logging
    OrderID submit_order(const OrderRequest& request) {
        context_->log_info("Submitting order: " + request.instrument);
        return context_->submit_order(request);
    }
};

} // namespace quantumliquidity::strategy
