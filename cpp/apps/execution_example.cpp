/**
 * @file execution_example.cpp
 * @brief Example demonstrating the Execution & Risk system
 *
 * This example shows:
 * - Setting up Position Manager, Risk Manager, and Execution Engine
 * - Registering a Mock Broker for testing
 * - Submitting orders with automatic risk checks
 * - Handling fills and position updates
 * - Real-time PnL tracking
 * - Risk limits enforcement
 */

#include "quantumliquidity/execution/execution_engine.hpp"
#include "quantumliquidity/execution/position_manager.hpp"
#include "quantumliquidity/execution/providers/mock_broker.hpp"
#include "quantumliquidity/risk/risk_manager.hpp"
#include "quantumliquidity/common/logger.hpp"
#include <iostream>
#include <iomanip>
#include <thread>
#include <chrono>

using namespace quantumliquidity::execution;
using namespace quantumliquidity::risk;

// Helper to generate order ID
std::string generate_order_id() {
    static int counter = 1;
    return "ORDER_" + std::to_string(counter++);
}

// Helper to create order request
OrderRequest create_order(
    const std::string& instrument,
    OrderSide side,
    double quantity,
    double price = 0.0,
    OrderType type = OrderType::MARKET
) {
    OrderRequest order;
    order.order_id = generate_order_id();
    order.instrument = instrument;
    order.side = side;
    order.type = type;
    order.quantity = quantity;
    order.price = price;
    order.tif = TimeInForce::DAY;
    order.strategy_id = "example_strategy";
    order.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    return order;
}

// Display position summary
void display_position(const PositionManager& pm, const std::string& instrument, double current_price) {
    auto pos = pm.get_position(instrument);

    if (std::abs(pos.quantity) < 1e-8) {
        std::cout << "[" << instrument << "] No position\n";
        return;
    }

    double upnl = pm.get_unrealized_pnl(instrument, current_price);
    double total_pnl = pos.realized_pnl + upnl;

    std::cout << "[" << instrument << "] "
              << "Qty: " << std::fixed << std::setprecision(2) << pos.quantity
              << " | Entry: " << std::fixed << std::setprecision(5) << pos.entry_price
              << " | Current: " << std::fixed << std::setprecision(5) << current_price
              << " | Realized PnL: " << std::fixed << std::setprecision(2) << pos.realized_pnl
              << " | Unrealized PnL: " << std::fixed << std::setprecision(2) << upnl
              << " | Total PnL: " << std::fixed << std::setprecision(2) << total_pnl
              << "\n";
}

// Display risk metrics
void display_risk_metrics(const RiskManager& rm) {
    auto metrics = rm.get_metrics();

    std::cout << "\n=== Risk Metrics ===\n";
    std::cout << "Total Exposure: $" << std::fixed << std::setprecision(2) << metrics.total_exposure << "\n";
    std::cout << "Account Utilization: " << std::fixed << std::setprecision(1) << metrics.account_utilization << "%\n";
    std::cout << "Daily PnL: $" << std::fixed << std::setprecision(2) << metrics.daily_pnl << "\n";
    std::cout << "Realized PnL: $" << std::fixed << std::setprecision(2) << metrics.realized_pnl << "\n";
    std::cout << "Unrealized PnL: $" << std::fixed << std::setprecision(2) << metrics.unrealized_pnl << "\n";
    std::cout << "Orders Submitted: " << metrics.orders_submitted_today << "\n";
    std::cout << "Orders Filled: " << metrics.orders_filled_today << "\n";
    std::cout << "Orders Rejected: " << metrics.orders_rejected_today << "\n";

    if (metrics.halt_active) {
        std::cout << "⚠️  TRADING HALTED: " << metrics.halt_reason << "\n";
    }
    std::cout << "===================\n\n";
}

int main() {
    std::cout << "QuantumLiquidity - Execution & Risk Example\n";
    std::cout << "============================================\n\n";

    // 1. Setup Risk Limits
    std::cout << "1. Configuring risk limits...\n";

    RiskLimits limits;
    limits.max_position_size = 1000.0;          // Max 1000 contracts per instrument
    limits.max_total_exposure = 50000.0;        // Max $50k total exposure
    limits.max_order_size = 500.0;              // Max 500 contracts per order
    limits.max_daily_loss = 2000.0;             // Stop trading if lose $2000
    limits.max_drawdown_from_high = 1000.0;     // Stop if drawdown > $1000 from daily high
    limits.max_orders_per_minute = 60;          // Rate limit: 60 orders/min
    limits.max_orders_per_day = 5000;           // Max 5000 orders per day
    limits.bankroll = 100000.0;                 // $100k account
    limits.min_free_capital_pct = 0.2;          // Keep 20% free

    std::cout << "   Bankroll: $" << limits.bankroll << "\n";
    std::cout << "   Max Daily Loss: $" << limits.max_daily_loss << "\n";
    std::cout << "   Max Position Size: " << limits.max_position_size << " contracts\n\n";

    // 2. Create components
    std::cout << "2. Initializing execution system...\n";

    PositionManager position_mgr;
    RiskManager risk_mgr(limits);
    risk_mgr.set_position_manager(&position_mgr);

    ExecutionEngine::Config engine_config;
    engine_config.enable_redis = false;  // Disable Redis for example

    ExecutionEngine engine(engine_config, &risk_mgr, &position_mgr);

    std::cout << "   ✓ Position Manager initialized\n";
    std::cout << "   ✓ Risk Manager initialized\n";
    std::cout << "   ✓ Execution Engine initialized\n\n";

    // 3. Setup Mock Broker
    std::cout << "3. Connecting to Mock Broker...\n";

    MockBroker::Config broker_config;
    broker_config.broker_name = "MockBroker";
    broker_config.fill_latency_ms = 100;        // 100ms fill delay
    broker_config.rejection_rate = 0.0;         // Never reject
    broker_config.enable_partial_fills = false;
    broker_config.slippage_bps = 1.0;           // 1bp slippage

    auto mock_broker = std::make_shared<MockBroker>(broker_config);

    // Set market prices
    mock_broker->set_market_price("EUR/USD", 1.1000);
    mock_broker->set_market_price("GBP/USD", 1.2500);
    mock_broker->set_market_price("USD/JPY", 110.50);

    engine.register_provider("mock", mock_broker);

    std::cout << "   ✓ Mock Broker connected\n";
    std::cout << "   EUR/USD: 1.1000\n";
    std::cout << "   GBP/USD: 1.2500\n";
    std::cout << "   USD/JPY: 110.50\n\n";

    // 4. Register callbacks
    engine.register_fill_callback([](const Fill& fill) {
        std::cout << "[FILL] " << fill.instrument
                  << " " << order_side_to_string(fill.side)
                  << " " << std::fixed << std::setprecision(2) << fill.quantity
                  << " @ " << std::fixed << std::setprecision(5) << fill.price
                  << " (order: " << fill.order_id << ")\n";
    });

    std::cout << "4. Submitting orders...\n\n";

    // 5. Example 1: Simple buy order
    std::cout << "=== Example 1: Simple Buy Order ===\n";
    {
        auto order = create_order("EUR/USD", OrderSide::BUY, 100.0);
        std::cout << "Submitting: BUY 100 EUR/USD @ MARKET\n";

        auto result = engine.submit_order(order);
        std::cout << "Result: " << order_status_to_string(result.status)
                  << " - " << result.reason << "\n";

        // Wait for fill
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        // Update market prices for unrealized PnL
        std::map<std::string, double> prices = {{"EUR/USD", 1.1000}};
        risk_mgr.update_market_prices(prices);

        display_position(position_mgr, "EUR/USD", 1.1000);
        display_risk_metrics(risk_mgr);
    }

    // 6. Example 2: Take profit
    std::cout << "=== Example 2: Price Moves Up, Take Profit ===\n";
    {
        // Simulate price movement
        double new_price = 1.1050;
        mock_broker->set_market_price("EUR/USD", new_price);

        std::cout << "EUR/USD moves to " << std::fixed << std::setprecision(5) << new_price << "\n";

        // Display unrealized profit
        std::map<std::string, double> prices = {{"EUR/USD", new_price}};
        risk_mgr.update_market_prices(prices);

        display_position(position_mgr, "EUR/USD", new_price);

        // Close position
        auto order = create_order("EUR/USD", OrderSide::SELL, 100.0);
        std::cout << "Submitting: SELL 100 EUR/USD @ MARKET (closing position)\n";

        auto result = engine.submit_order(order);
        std::cout << "Result: " << order_status_to_string(result.status) << "\n";

        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        risk_mgr.update_market_prices(prices);
        display_position(position_mgr, "EUR/USD", new_price);
        display_risk_metrics(risk_mgr);
    }

    // 7. Example 3: Short trade (loss)
    std::cout << "=== Example 3: Short Trade with Loss ===\n";
    {
        auto order = create_order("GBP/USD", OrderSide::SELL, 50.0);
        std::cout << "Submitting: SELL 50 GBP/USD @ MARKET (going short)\n";

        auto result = engine.submit_order(order);
        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        std::map<std::string, double> prices = {
            {"EUR/USD", 1.1050},
            {"GBP/USD", 1.2500}
        };
        risk_mgr.update_market_prices(prices);
        display_position(position_mgr, "GBP/USD", 1.2500);

        // Price moves against us
        double new_price = 1.2600;
        mock_broker->set_market_price("GBP/USD", new_price);
        prices["GBP/USD"] = new_price;
        risk_mgr.update_market_prices(prices);

        std::cout << "GBP/USD moves to " << std::fixed << std::setprecision(5) << new_price << " (against us!)\n";
        display_position(position_mgr, "GBP/USD", new_price);

        // Close at loss
        auto close_order = create_order("GBP/USD", OrderSide::BUY, 50.0);
        std::cout << "Submitting: BUY 50 GBP/USD @ MARKET (closing at loss)\n";
        engine.submit_order(close_order);

        std::this_thread::sleep_for(std::chrono::milliseconds(200));

        risk_mgr.update_market_prices(prices);
        display_position(position_mgr, "GBP/USD", new_price);
        display_risk_metrics(risk_mgr);
    }

    // 8. Example 4: Risk rejection
    std::cout << "=== Example 4: Risk Rejection (Order Too Large) ===\n";
    {
        auto order = create_order("USD/JPY", OrderSide::BUY, 2000.0);  // Exceeds limit
        std::cout << "Submitting: BUY 2000 USD/JPY @ MARKET (exceeds position limit!)\n";

        auto result = engine.submit_order(order);
        std::cout << "Result: " << order_status_to_string(result.status)
                  << " - " << result.reason << "\n\n";

        display_risk_metrics(risk_mgr);
    }

    // 9. Example 5: Multiple instruments
    std::cout << "=== Example 5: Portfolio with Multiple Instruments ===\n";
    {
        // Build a portfolio
        auto order1 = create_order("EUR/USD", OrderSide::BUY, 100.0);
        auto order2 = create_order("GBP/USD", OrderSide::BUY, 75.0);
        auto order3 = create_order("USD/JPY", OrderSide::SELL, 50.0);

        std::cout << "Submitting 3 orders to build portfolio...\n";
        engine.submit_order(order1);
        engine.submit_order(order2);
        engine.submit_order(order3);

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        // Update prices
        std::map<std::string, double> prices = {
            {"EUR/USD", 1.1060},
            {"GBP/USD", 1.2520},
            {"USD/JPY", 110.30}
        };
        risk_mgr.update_market_prices(prices);

        std::cout << "\nPortfolio Summary:\n";
        std::cout << "------------------\n";
        display_position(position_mgr, "EUR/USD", prices["EUR/USD"]);
        display_position(position_mgr, "GBP/USD", prices["GBP/USD"]);
        display_position(position_mgr, "USD/JPY", prices["USD/JPY"]);

        auto pos_stats = position_mgr.get_stats(prices);
        std::cout << "\nPortfolio Stats:\n";
        std::cout << "Active Positions: " << pos_stats.num_positions << "\n";
        std::cout << "Total Realized PnL: $" << std::fixed << std::setprecision(2)
                  << pos_stats.total_realized_pnl << "\n";
        std::cout << "Total Unrealized PnL: $" << std::fixed << std::setprecision(2)
                  << pos_stats.total_unrealized_pnl << "\n";
        std::cout << "Total Commission: $" << std::fixed << std::setprecision(2)
                  << pos_stats.total_commission_paid << "\n\n";

        display_risk_metrics(risk_mgr);
    }

    // 10. Cleanup
    std::cout << "10. Shutting down...\n";
    engine.shutdown();

    auto engine_stats = engine.get_stats();
    std::cout << "\nFinal Statistics:\n";
    std::cout << "-----------------\n";
    std::cout << "Orders Submitted: " << engine_stats.total_orders_submitted << "\n";
    std::cout << "Orders Filled: " << engine_stats.total_orders_filled << "\n";
    std::cout << "Orders Rejected: " << engine_stats.total_orders_rejected << "\n";
    std::cout << "Total Volume: " << std::fixed << std::setprecision(2)
              << engine_stats.total_volume_traded << "\n";

    auto broker_stats = mock_broker->get_stats();
    std::cout << "\nMock Broker Stats:\n";
    std::cout << "Orders Received: " << broker_stats.orders_received << "\n";
    std::cout << "Orders Filled: " << broker_stats.orders_filled << "\n";
    std::cout << "Fills Generated: " << broker_stats.fills_generated << "\n";

    std::cout << "\n✓ Example completed successfully!\n";

    return 0;
}
