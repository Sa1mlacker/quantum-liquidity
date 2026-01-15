/**
 * Base Strategy Interface
 * All trading strategies inherit from this
 */

#pragma once

#include "quantumliquidity/market_data/tick.hpp"
#include "quantumliquidity/market_data/ohlcv.hpp"
#include "quantumliquidity/execution/order.hpp"
#include "quantumliquidity/execution/position_manager.hpp"
#include <string>
#include <memory>
#include <vector>

namespace quantumliquidity::strategy {

enum class StrategyState {
    IDLE,           // Not running
    STARTING,       // Initialization
    RUNNING,        // Active trading
    STOPPING,       // Shutdown
    STOPPED,        // Fully stopped
    ERROR           // Error state
};

struct StrategyConfig {
    std::string name;
    std::vector<std::string> instruments;
    double max_position_size;
    double max_daily_loss;
    bool enabled;
};

class Strategy {
public:
    explicit Strategy(const StrategyConfig& config);
    virtual ~Strategy() = default;

    // Lifecycle
    virtual void on_start() = 0;
    virtual void on_stop() = 0;

    // Market data callbacks
    virtual void on_tick(const market_data::Tick& tick) = 0;
    virtual void on_bar(const market_data::OHLCV& bar) = 0;

    // Execution callbacks
    virtual void on_fill(const execution::Fill& fill) = 0;
    virtual void on_order_update(const execution::Order& order) = 0;

    // State management
    void start();
    void stop();
    StrategyState get_state() const { return state_; }
    std::string get_name() const { return config_.name; }

    // Position access
    void set_position_manager(std::shared_ptr<execution::PositionManager> pm) {
        position_manager_ = pm;
    }

protected:
    // Helper methods for derived strategies
    void submit_order(execution::OrderRequest request);
    double get_position(const std::string& instrument) const;
    double get_unrealized_pnl(const std::string& instrument) const;

    StrategyConfig config_;
    std::shared_ptr<execution::PositionManager> position_manager_;

private:
    StrategyState state_ = StrategyState::IDLE;
    std::function<void(execution::OrderRequest)> order_callback_;

public:
    void set_order_callback(std::function<void(execution::OrderRequest)> callback) {
        order_callback_ = callback;
    }
};

} // namespace quantumliquidity::strategy
