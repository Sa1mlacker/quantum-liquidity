/**
 * Strategy Manager
 * Manages multiple trading strategies and routes market data
 */

#pragma once

#include "quantumliquidity/strategy/strategy.hpp"
#include "quantumliquidity/execution/execution_engine.hpp"
#include "quantumliquidity/market_data/tick.hpp"
#include "quantumliquidity/market_data/ohlcv.hpp"
#include <memory>
#include <vector>
#include <map>
#include <mutex>

namespace quantumliquidity::strategy {

class StrategyManager {
public:
    StrategyManager(
        std::shared_ptr<execution::ExecutionEngine> exec_engine,
        std::shared_ptr<execution::PositionManager> pos_manager
    );

    // Strategy lifecycle
    void add_strategy(std::shared_ptr<Strategy> strategy);
    void remove_strategy(const std::string& name);
    void start_all();
    void stop_all();
    void start_strategy(const std::string& name);
    void stop_strategy(const std::string& name);

    // Market data routing
    void on_tick(const market_data::Tick& tick);
    void on_bar(const market_data::OHLCV& bar);

    // Execution callbacks
    void on_fill(const execution::Fill& fill);
    void on_order_update(const execution::Order& order);

    // Status
    std::vector<std::string> get_active_strategies() const;
    std::shared_ptr<Strategy> get_strategy(const std::string& name) const;

private:
    std::shared_ptr<execution::ExecutionEngine> exec_engine_;
    std::shared_ptr<execution::PositionManager> pos_manager_;

    std::map<std::string, std::shared_ptr<Strategy>> strategies_;
    mutable std::mutex mutex_;

    // Track which instruments each strategy is interested in
    std::map<std::string, std::vector<std::string>> instrument_to_strategies_;

    void update_instrument_mapping();
};

} // namespace quantumliquidity::strategy
