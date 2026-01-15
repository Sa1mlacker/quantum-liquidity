/**
 * Strategy Manager Implementation
 */

#include "quantumliquidity/strategy/strategy_manager.hpp"
#include <iostream>
#include <algorithm>

namespace quantumliquidity::strategy {

StrategyManager::StrategyManager(
    std::shared_ptr<execution::ExecutionEngine> exec_engine,
    std::shared_ptr<execution::PositionManager> pos_manager
)
    : exec_engine_(exec_engine)
    , pos_manager_(pos_manager)
{
}

void StrategyManager::add_strategy(std::shared_ptr<Strategy> strategy) {
    std::lock_guard<std::mutex> lock(mutex_);

    std::string name = strategy->get_name();
    if (strategies_.find(name) != strategies_.end()) {
        throw std::runtime_error("Strategy already exists: " + name);
    }

    // Set callbacks
    strategy->set_position_manager(pos_manager_);
    strategy->set_order_callback([this](execution::OrderRequest request) {
        exec_engine_->submit_order(request);
    });

    strategies_[name] = strategy;
    update_instrument_mapping();

    std::cout << "[StrategyManager] Added strategy: " << name << "\n";
}

void StrategyManager::remove_strategy(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = strategies_.find(name);
    if (it == strategies_.end()) {
        return;
    }

    // Stop if running
    if (it->second->get_state() == StrategyState::RUNNING) {
        it->second->stop();
    }

    strategies_.erase(it);
    update_instrument_mapping();

    std::cout << "[StrategyManager] Removed strategy: " << name << "\n";
}

void StrategyManager::start_all() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::cout << "[StrategyManager] Starting all strategies (" << strategies_.size() << ")\n";

    for (auto& [name, strategy] : strategies_) {
        try {
            strategy->start();
            std::cout << "[StrategyManager] Started: " << name << "\n";
        } catch (const std::exception& e) {
            std::cerr << "[StrategyManager] Failed to start " << name << ": " << e.what() << "\n";
        }
    }
}

void StrategyManager::stop_all() {
    std::lock_guard<std::mutex> lock(mutex_);

    std::cout << "[StrategyManager] Stopping all strategies\n";

    for (auto& [name, strategy] : strategies_) {
        try {
            strategy->stop();
            std::cout << "[StrategyManager] Stopped: " << name << "\n";
        } catch (const std::exception& e) {
            std::cerr << "[StrategyManager] Failed to stop " << name << ": " << e.what() << "\n";
        }
    }
}

void StrategyManager::start_strategy(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = strategies_.find(name);
    if (it == strategies_.end()) {
        throw std::runtime_error("Strategy not found: " + name);
    }

    it->second->start();
    std::cout << "[StrategyManager] Started strategy: " << name << "\n";
}

void StrategyManager::stop_strategy(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = strategies_.find(name);
    if (it == strategies_.end()) {
        throw std::runtime_error("Strategy not found: " + name);
    }

    it->second->stop();
    std::cout << "[StrategyManager] Stopped strategy: " << name << "\n";
}

void StrategyManager::on_tick(const market_data::Tick& tick) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Find strategies interested in this instrument
    auto it = instrument_to_strategies_.find(tick.instrument);
    if (it == instrument_to_strategies_.end()) {
        return;
    }

    // Route tick to interested strategies
    for (const auto& strategy_name : it->second) {
        auto strat_it = strategies_.find(strategy_name);
        if (strat_it != strategies_.end() &&
            strat_it->second->get_state() == StrategyState::RUNNING) {
            strat_it->second->on_tick(tick);
        }
    }
}

void StrategyManager::on_bar(const market_data::OHLCV& bar) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Find strategies interested in this instrument
    auto it = instrument_to_strategies_.find(bar.instrument);
    if (it == instrument_to_strategies_.end()) {
        return;
    }

    // Route bar to interested strategies
    for (const auto& strategy_name : it->second) {
        auto strat_it = strategies_.find(strategy_name);
        if (strat_it != strategies_.end() &&
            strat_it->second->get_state() == StrategyState::RUNNING) {
            strat_it->second->on_bar(bar);
        }
    }
}

void StrategyManager::on_fill(const execution::Fill& fill) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Broadcast fill to all running strategies
    for (auto& [name, strategy] : strategies_) {
        if (strategy->get_state() == StrategyState::RUNNING) {
            strategy->on_fill(fill);
        }
    }
}

void StrategyManager::on_order_update(const execution::Order& order) {
    std::lock_guard<std::mutex> lock(mutex_);

    // Broadcast order update to all running strategies
    for (auto& [name, strategy] : strategies_) {
        if (strategy->get_state() == StrategyState::RUNNING) {
            strategy->on_order_update(order);
        }
    }
}

std::vector<std::string> StrategyManager::get_active_strategies() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> active;
    for (const auto& [name, strategy] : strategies_) {
        if (strategy->get_state() == StrategyState::RUNNING) {
            active.push_back(name);
        }
    }
    return active;
}

std::shared_ptr<Strategy> StrategyManager::get_strategy(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = strategies_.find(name);
    return it != strategies_.end() ? it->second : nullptr;
}

void StrategyManager::update_instrument_mapping() {
    instrument_to_strategies_.clear();

    for (const auto& [name, strategy] : strategies_) {
        // Note: We'd need to expose instruments from Strategy base class
        // For now, this is a simplified version
    }
}

} // namespace quantumliquidity::strategy
