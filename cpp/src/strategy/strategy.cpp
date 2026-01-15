/**
 * Strategy Base Implementation
 */

#include "quantumliquidity/strategy/strategy.hpp"
#include <stdexcept>

namespace quantumliquidity::strategy {

Strategy::Strategy(const StrategyConfig& config) : config_(config) {}

void Strategy::start() {
    if (state_ == StrategyState::RUNNING) {
        throw std::runtime_error("Strategy already running");
    }

    state_ = StrategyState::STARTING;
    on_start();
    state_ = StrategyState::RUNNING;
}

void Strategy::stop() {
    if (state_ != StrategyState::RUNNING) {
        return;
    }

    state_ = StrategyState::STOPPING;
    on_stop();
    state_ = StrategyState::STOPPED;
}

void Strategy::submit_order(execution::OrderRequest request) {
    if (state_ != StrategyState::RUNNING) {
        throw std::runtime_error("Strategy not running, cannot submit order");
    }

    if (!order_callback_) {
        throw std::runtime_error("Order callback not set");
    }

    order_callback_(request);
}

double Strategy::get_position(const std::string& instrument) const {
    if (!position_manager_) {
        return 0.0;
    }

    auto pos = position_manager_->get_position(instrument);
    return pos ? pos->quantity : 0.0;
}

double Strategy::get_unrealized_pnl(const std::string& instrument) const {
    if (!position_manager_) {
        return 0.0;
    }

    auto pos = position_manager_->get_position(instrument);
    return pos ? pos->unrealized_pnl : 0.0;
}

} // namespace quantumliquidity::strategy
