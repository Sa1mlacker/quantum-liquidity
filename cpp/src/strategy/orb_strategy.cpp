/**
 * ORB Strategy Implementation
 */

#include "quantumliquidity/strategy/orb_strategy.hpp"
#include <iostream>
#include <cmath>

namespace quantumliquidity::strategy {

ORBStrategy::ORBStrategy(const ORBConfig& config)
    : Strategy(config)
    , orb_config_(config)
    , analyzer_(config.period_minutes)
{
}

void ORBStrategy::on_start() {
    std::cout << "[ORBStrategy] Starting strategy: " << config_.name << "\n";
    std::cout << "[ORBStrategy] Period: " << orb_config_.period_minutes << " minutes\n";
    std::cout << "[ORBStrategy] Instruments: ";
    for (const auto& inst : config_.instruments) {
        std::cout << inst << " ";
    }
    std::cout << "\n";

    // Initialize instrument states
    for (const auto& inst : config_.instruments) {
        instrument_states_[inst] = InstrumentState{};
    }
}

void ORBStrategy::on_stop() {
    std::cout << "[ORBStrategy] Stopping strategy: " << config_.name << "\n";

    // Close all positions
    for (const auto& inst : config_.instruments) {
        double position = get_position(inst);
        if (std::abs(position) > 1e-6) {
            execution::OrderRequest close_order;
            close_order.instrument = inst;
            close_order.side = position > 0 ? execution::OrderSide::SELL : execution::OrderSide::BUY;
            close_order.quantity = std::abs(position);
            close_order.order_type = execution::OrderType::MARKET;

            std::cout << "[ORBStrategy] Closing position: " << inst << " qty=" << position << "\n";
            submit_order(close_order);
        }
    }
}

void ORBStrategy::on_tick(const market_data::Tick& tick) {
    if (!is_trading_hours()) {
        return;
    }

    auto it = instrument_states_.find(tick.instrument);
    if (it == instrument_states_.end()) {
        return;
    }

    auto& state = it->second;

    // Check if we need to calculate opening range
    auto now = std::chrono::system_clock::now();

    // If new session, reset state
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    auto now_tm = *std::localtime(&now_time_t);

    if (now_tm.tm_hour == orb_config_.session_start_hour &&
        now_tm.tm_min == orb_config_.session_start_minute) {
        reset_daily_state();
    }

    // Update opening range during period
    if (!state.or_calculated) {
        if (state.or_high == 0.0) {
            state.or_high = tick.price;
            state.or_low = tick.price;
        } else {
            state.or_high = std::max(state.or_high, tick.price);
            state.or_low = std::min(state.or_low, tick.price);
        }

        // Check if period ended
        auto session_start_time_t = std::chrono::system_clock::to_time_t(state.session_start);
        auto session_start_tm = *std::localtime(&session_start_time_t);

        int elapsed_minutes = (now_tm.tm_hour * 60 + now_tm.tm_min) -
                             (session_start_tm.tm_hour * 60 + session_start_tm.tm_min);

        if (elapsed_minutes >= orb_config_.period_minutes) {
            state.or_calculated = true;
            std::cout << "[ORBStrategy] OR calculated for " << tick.instrument
                      << " High=" << state.or_high << " Low=" << state.or_low
                      << " Range=" << (state.or_high - state.or_low) << "\n";
        }
    } else {
        // Check for breakouts
        check_breakout(tick.instrument, tick.price);
    }
}

void ORBStrategy::on_bar(const market_data::OHLCV& bar) {
    // Can also use bar data for breakout detection
}

void ORBStrategy::on_fill(const execution::Fill& fill) {
    std::cout << "[ORBStrategy] Fill: " << fill.instrument
              << " " << (fill.side == execution::OrderSide::BUY ? "BUY" : "SELL")
              << " qty=" << fill.quantity << " price=" << fill.price << "\n";
}

void ORBStrategy::on_order_update(const execution::Order& order) {
    std::cout << "[ORBStrategy] Order update: " << order.instrument
              << " status=" << static_cast<int>(order.status) << "\n";
}

void ORBStrategy::check_breakout(const std::string& instrument, double price) {
    auto& state = instrument_states_[instrument];

    if (!state.or_calculated) {
        return;
    }

    // Check for high breakout
    if (orb_config_.trade_high_breakout &&
        !state.high_breakout_taken &&
        price > state.or_high + orb_config_.breakout_threshold) {

        // Check if we already have max positions
        double current_pos = get_position(instrument);
        if (std::abs(current_pos) >= orb_config_.position_size * orb_config_.max_positions) {
            return;
        }

        // Submit long order
        execution::OrderRequest order;
        order.instrument = instrument;
        order.side = execution::OrderSide::BUY;
        order.quantity = orb_config_.position_size;
        order.order_type = execution::OrderType::MARKET;

        std::cout << "[ORBStrategy] HIGH BREAKOUT detected: " << instrument
                  << " price=" << price << " OR_high=" << state.or_high << "\n";

        submit_order(order);
        state.high_breakout_taken = true;
    }

    // Check for low breakout
    if (orb_config_.trade_low_breakout &&
        !state.low_breakout_taken &&
        price < state.or_low - orb_config_.breakout_threshold) {

        // Check if we already have max positions
        double current_pos = get_position(instrument);
        if (std::abs(current_pos) >= orb_config_.position_size * orb_config_.max_positions) {
            return;
        }

        // Submit short order
        execution::OrderRequest order;
        order.instrument = instrument;
        order.side = execution::OrderSide::SELL;
        order.quantity = orb_config_.position_size;
        order.order_type = execution::OrderType::MARKET;

        std::cout << "[ORBStrategy] LOW BREAKOUT detected: " << instrument
                  << " price=" << price << " OR_low=" << state.or_low << "\n";

        submit_order(order);
        state.low_breakout_taken = true;
    }
}

bool ORBStrategy::is_trading_hours() const {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    auto now_tm = *std::localtime(&now_time_t);

    int current_minutes = now_tm.tm_hour * 60 + now_tm.tm_min;
    int start_minutes = orb_config_.session_start_hour * 60 + orb_config_.session_start_minute;
    int end_minutes = orb_config_.session_end_hour * 60 + orb_config_.session_end_minute;

    return current_minutes >= start_minutes && current_minutes < end_minutes;
}

void ORBStrategy::reset_daily_state() {
    std::cout << "[ORBStrategy] Resetting daily state\n";

    for (auto& [inst, state] : instrument_states_) {
        state.or_high = 0.0;
        state.or_low = 0.0;
        state.or_calculated = false;
        state.high_breakout_taken = false;
        state.low_breakout_taken = false;
        state.session_start = std::chrono::system_clock::now();
    }
}

} // namespace quantumliquidity::strategy
