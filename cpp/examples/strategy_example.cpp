/**
 * Strategy Framework Example
 * Demonstrates ORB breakout strategy with live market data
 */

#include "quantumliquidity/strategy/strategy_manager.hpp"
#include "quantumliquidity/strategy/orb_strategy.hpp"
#include "quantumliquidity/execution/execution_engine.hpp"
#include "quantumliquidity/execution/position_manager.hpp"
#include "quantumliquidity/execution/mock_broker.hpp"
#include "quantumliquidity/market_data/tick.hpp"
#include <iostream>
#include <thread>
#include <chrono>

using namespace quantumliquidity;

int main() {
    std::cout << "=== QuantumLiquidity Strategy Example ===\n\n";

    // 1. Setup execution infrastructure
    auto pos_manager = std::make_shared<execution::PositionManager>();
    auto mock_broker = std::make_shared<execution::MockBroker>();
    auto exec_engine = std::make_shared<execution::ExecutionEngine>(mock_broker);

    // Connect execution callbacks
    exec_engine->set_fill_callback([&](const execution::Fill& fill) {
        pos_manager->on_fill(fill);
    });

    // 2. Create strategy manager
    auto strategy_manager = std::make_shared<strategy::StrategyManager>(
        exec_engine,
        pos_manager
    );

    // Setup callbacks
    exec_engine->set_fill_callback([&](const execution::Fill& fill) {
        pos_manager->on_fill(fill);
        strategy_manager->on_fill(fill);
    });

    exec_engine->set_order_update_callback([&](const execution::Order& order) {
        strategy_manager->on_order_update(order);
    });

    // 3. Configure ORB strategy
    strategy::ORBConfig config;
    config.name = "ES_ORB_30min";
    config.instruments = {"ES"};  // E-mini S&P 500
    config.max_position_size = 1.0;
    config.max_daily_loss = 5000.0;
    config.enabled = true;

    // ORB specific
    config.period_minutes = 30;
    config.breakout_threshold = 0.25;  // 0.25 points
    config.max_positions = 1;
    config.position_size = 1.0;
    config.trade_high_breakout = true;
    config.trade_low_breakout = true;

    // Session times (9:30 AM - 4:00 PM ET)
    config.session_start_hour = 9;
    config.session_start_minute = 30;
    config.session_end_hour = 16;
    config.session_end_minute = 0;

    auto orb_strategy = std::make_shared<strategy::ORBStrategy>(config);

    // 4. Add strategy to manager
    strategy_manager->add_strategy(orb_strategy);

    std::cout << "Strategy configured:\n";
    std::cout << "  Name: " << config.name << "\n";
    std::cout << "  Instrument: ES\n";
    std::cout << "  ORB Period: " << config.period_minutes << " minutes\n";
    std::cout << "  Breakout Threshold: " << config.breakout_threshold << "\n\n";

    // 5. Start strategy
    strategy_manager->start_all();

    std::cout << "Strategy started. Simulating market data...\n\n";

    // 6. Simulate market data (Opening Range)
    double base_price = 4750.0;

    // Opening range: 9:30 - 10:00 (30 minutes)
    std::cout << "=== Opening Range (9:30 - 10:00) ===\n";
    for (int i = 0; i < 30; ++i) {
        market_data::Tick tick;
        tick.instrument = "ES";
        tick.price = base_price + (std::rand() % 10 - 5) * 0.25;  // Random within ±1.25 points
        tick.bid = tick.price - 0.25;
        tick.ask = tick.price + 0.25;
        tick.volume = 100;
        tick.timestamp_ns = std::chrono::system_clock::now().time_since_epoch().count();

        strategy_manager->on_tick(tick);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "\n=== After Opening Range ===\n";
    std::cout << "OR High: ~" << (base_price + 1.25) << "\n";
    std::cout << "OR Low: ~" << (base_price - 1.25) << "\n\n";

    // 7. Simulate breakout
    std::cout << "=== Simulating HIGH Breakout ===\n";
    for (int i = 0; i < 20; ++i) {
        market_data::Tick tick;
        tick.instrument = "ES";
        tick.price = base_price + 1.5 + i * 0.25;  // Breaking above OR high
        tick.bid = tick.price - 0.25;
        tick.ask = tick.price + 0.25;
        tick.volume = 100;
        tick.timestamp_ns = std::chrono::system_clock::now().time_since_epoch().count();

        strategy_manager->on_tick(tick);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 8. Check positions
    std::cout << "\n=== Current Position ===\n";
    auto position = pos_manager->get_position("ES");
    if (position) {
        std::cout << "Instrument: " << position->instrument << "\n";
        std::cout << "Quantity: " << position->quantity << "\n";
        std::cout << "Entry Price: " << position->entry_price << "\n";
        std::cout << "Unrealized PnL: $" << position->unrealized_pnl << "\n";
        std::cout << "Realized PnL: $" << position->realized_pnl << "\n";
    } else {
        std::cout << "No position\n";
    }

    // 9. Simulate price moving in profit
    std::cout << "\n=== Price Moving Higher ===\n";
    for (int i = 0; i < 10; ++i) {
        market_data::Tick tick;
        tick.instrument = "ES";
        tick.price = base_price + 6.0 + i * 0.5;  // Continue higher
        tick.bid = tick.price - 0.25;
        tick.ask = tick.price + 0.25;
        tick.volume = 100;
        tick.timestamp_ns = std::chrono::system_clock::now().time_since_epoch().count();

        strategy_manager->on_tick(tick);

        // Update position PnL
        if (position) {
            pos_manager->update_position_price("ES", tick.price);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 10. Final position check
    std::cout << "\n=== Final Position ===\n";
    position = pos_manager->get_position("ES");
    if (position) {
        std::cout << "Quantity: " << position->quantity << "\n";
        std::cout << "Entry Price: " << position->entry_price << "\n";
        std::cout << "Current Price: " << (base_price + 10.5) << "\n";
        std::cout << "Unrealized PnL: $" << position->unrealized_pnl << "\n";
    }

    // 11. Stop strategy
    std::cout << "\n=== Stopping Strategy ===\n";
    strategy_manager->stop_all();

    // Check final position after close
    std::cout << "\n=== Position After Close ===\n";
    position = pos_manager->get_position("ES");
    if (position) {
        std::cout << "Quantity: " << position->quantity << " (should be 0)\n";
        std::cout << "Realized PnL: $" << position->realized_pnl << "\n";
    }

    std::cout << "\n=== Example Complete ===\n";
    return 0;
}
