#include <gtest/gtest.h>
#include "quantumliquidity/execution/position_manager.hpp"
#include "quantumliquidity/execution/execution_engine.hpp"
#include "quantumliquidity/execution/providers/mock_broker.hpp"
#include "quantumliquidity/risk/risk_manager.hpp"
#include <chrono>
#include <thread>

using namespace quantumliquidity::execution;
using namespace quantumliquidity::risk;

// Helper to create order request
OrderRequest create_test_order(
    const std::string& order_id,
    const std::string& instrument,
    OrderSide side,
    double quantity,
    double price = 0.0,
    OrderType type = OrderType::MARKET
) {
    OrderRequest order;
    order.order_id = order_id;
    order.instrument = instrument;
    order.side = side;
    order.type = type;
    order.quantity = quantity;
    order.price = price;
    order.tif = TimeInForce::DAY;
    order.strategy_id = "test_strategy";
    order.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    return order;
}

// Helper to create fill
Fill create_test_fill(
    const std::string& fill_id,
    const std::string& order_id,
    const std::string& instrument,
    OrderSide side,
    double quantity,
    double price
) {
    Fill fill;
    fill.fill_id = fill_id;
    fill.order_id = order_id;
    fill.instrument = instrument;
    fill.side = side;
    fill.quantity = quantity;
    fill.price = price;
    fill.commission = 0.0;
    fill.timestamp_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
    return fill;
}

// ==================== Position Manager Tests ====================

TEST(PositionManagerTest, NewPositionLong) {
    PositionManager pm;

    auto fill = create_test_fill("F1", "O1", "EUR/USD", OrderSide::BUY, 100.0, 1.1000);
    pm.on_fill(fill);

    auto pos = pm.get_position("EUR/USD");
    EXPECT_EQ(pos.quantity, 100.0);
    EXPECT_DOUBLE_EQ(pos.entry_price, 1.1000);
    EXPECT_DOUBLE_EQ(pos.realized_pnl, 0.0);
}

TEST(PositionManagerTest, NewPositionShort) {
    PositionManager pm;

    auto fill = create_test_fill("F1", "O1", "EUR/USD", OrderSide::SELL, 100.0, 1.1000);
    pm.on_fill(fill);

    auto pos = pm.get_position("EUR/USD");
    EXPECT_EQ(pos.quantity, -100.0);
    EXPECT_DOUBLE_EQ(pos.entry_price, 1.1000);
}

TEST(PositionManagerTest, IncreasePosition) {
    PositionManager pm;

    // First fill: buy 100 @ 1.1000
    auto fill1 = create_test_fill("F1", "O1", "EUR/USD", OrderSide::BUY, 100.0, 1.1000);
    pm.on_fill(fill1);

    // Second fill: buy 50 @ 1.1100
    auto fill2 = create_test_fill("F2", "O2", "EUR/USD", OrderSide::BUY, 50.0, 1.1100);
    pm.on_fill(fill2);

    auto pos = pm.get_position("EUR/USD");
    EXPECT_EQ(pos.quantity, 150.0);

    // Weighted average: (100 * 1.1 + 50 * 1.11) / 150 = 1.1033333
    EXPECT_NEAR(pos.entry_price, 1.1033333, 1e-6);
}

TEST(PositionManagerTest, ReducePosition) {
    PositionManager pm;

    // Buy 100 @ 1.1000
    auto fill1 = create_test_fill("F1", "O1", "EUR/USD", OrderSide::BUY, 100.0, 1.1000);
    pm.on_fill(fill1);

    // Sell 60 @ 1.1100 (profit: 60 * 0.01 = 0.6)
    auto fill2 = create_test_fill("F2", "O2", "EUR/USD", OrderSide::SELL, 60.0, 1.1100);
    pm.on_fill(fill2);

    auto pos = pm.get_position("EUR/USD");
    EXPECT_EQ(pos.quantity, 40.0);
    EXPECT_DOUBLE_EQ(pos.entry_price, 1.1000);  // Entry price unchanged
    EXPECT_NEAR(pos.realized_pnl, 6.0, 1e-6);    // 60 * (1.11 - 1.10) = 6.0
}

TEST(PositionManagerTest, ClosePosition) {
    PositionManager pm;

    // Buy 100 @ 1.1000
    auto fill1 = create_test_fill("F1", "O1", "EUR/USD", OrderSide::BUY, 100.0, 1.1000);
    pm.on_fill(fill1);

    // Sell 100 @ 1.1050 (profit: 100 * 0.005 = 0.5)
    auto fill2 = create_test_fill("F2", "O2", "EUR/USD", OrderSide::SELL, 100.0, 1.1050);
    pm.on_fill(fill2);

    auto pos = pm.get_position("EUR/USD");
    EXPECT_NEAR(pos.quantity, 0.0, 1e-8);
    EXPECT_NEAR(pos.realized_pnl, 5.0, 1e-6);
}

TEST(PositionManagerTest, ReversePosition) {
    PositionManager pm;

    // Buy 100 @ 1.1000
    auto fill1 = create_test_fill("F1", "O1", "EUR/USD", OrderSide::BUY, 100.0, 1.1000);
    pm.on_fill(fill1);

    // Sell 150 @ 1.1100 (close 100 + open -50 short)
    auto fill2 = create_test_fill("F2", "O2", "EUR/USD", OrderSide::SELL, 150.0, 1.1100);
    pm.on_fill(fill2);

    auto pos = pm.get_position("EUR/USD");
    EXPECT_EQ(pos.quantity, -50.0);
    EXPECT_DOUBLE_EQ(pos.entry_price, 1.1100);  // New entry for short
    EXPECT_NEAR(pos.realized_pnl, 10.0, 1e-6);  // 100 * (1.11 - 1.10) = 10.0
}

TEST(PositionManagerTest, UnrealizedPnL_Long) {
    PositionManager pm;

    auto fill = create_test_fill("F1", "O1", "EUR/USD", OrderSide::BUY, 100.0, 1.1000);
    pm.on_fill(fill);

    // Price goes up
    double upnl = pm.get_unrealized_pnl("EUR/USD", 1.1050);
    EXPECT_NEAR(upnl, 5.0, 1e-6);  // 100 * (1.105 - 1.10) = 5.0

    // Price goes down
    upnl = pm.get_unrealized_pnl("EUR/USD", 1.0950);
    EXPECT_NEAR(upnl, -5.0, 1e-6);  // 100 * (1.095 - 1.10) = -5.0
}

TEST(PositionManagerTest, UnrealizedPnL_Short) {
    PositionManager pm;

    auto fill = create_test_fill("F1", "O1", "EUR/USD", OrderSide::SELL, 100.0, 1.1000);
    pm.on_fill(fill);

    // Price goes down (profit for short)
    double upnl = pm.get_unrealized_pnl("EUR/USD", 1.0950);
    EXPECT_NEAR(upnl, 5.0, 1e-6);  // -100 * (1.095 - 1.10) = 5.0

    // Price goes up (loss for short)
    upnl = pm.get_unrealized_pnl("EUR/USD", 1.1050);
    EXPECT_NEAR(upnl, -5.0, 1e-6);  // -100 * (1.105 - 1.10) = -5.0
}

TEST(PositionManagerTest, TotalExposure) {
    PositionManager pm;

    auto fill1 = create_test_fill("F1", "O1", "EUR/USD", OrderSide::BUY, 100.0, 1.1000);
    pm.on_fill(fill1);

    auto fill2 = create_test_fill("F2", "O2", "GBP/USD", OrderSide::SELL, 50.0, 1.2500);
    pm.on_fill(fill2);

    std::map<std::string, double> prices = {
        {"EUR/USD", 1.1100},
        {"GBP/USD", 1.2600}
    };

    double exposure = pm.get_total_exposure(prices);
    // EUR: 100 * 1.11 = 111.0
    // GBP: 50 * 1.26 = 63.0
    // Total: 174.0
    EXPECT_NEAR(exposure, 174.0, 1e-6);
}

TEST(PositionManagerTest, MultipleInstruments) {
    PositionManager pm;

    auto fill1 = create_test_fill("F1", "O1", "EUR/USD", OrderSide::BUY, 100.0, 1.1000);
    pm.on_fill(fill1);

    auto fill2 = create_test_fill("F2", "O2", "GBP/USD", OrderSide::BUY, 50.0, 1.2500);
    pm.on_fill(fill2);

    auto fill3 = create_test_fill("F3", "O3", "USD/JPY", OrderSide::SELL, 75.0, 110.50);
    pm.on_fill(fill3);

    EXPECT_TRUE(pm.has_position("EUR/USD"));
    EXPECT_TRUE(pm.has_position("GBP/USD"));
    EXPECT_TRUE(pm.has_position("USD/JPY"));
    EXPECT_FALSE(pm.has_position("AUD/USD"));

    auto positions = pm.get_all_positions();
    EXPECT_EQ(positions.size(), 3);
}

// ==================== Risk Manager Tests ====================

TEST(RiskManagerTest, ValidOrder) {
    RiskLimits limits;
    limits.max_position_size = 1000.0;
    limits.max_total_exposure = 100000.0;
    limits.max_order_size = 500.0;
    limits.max_daily_loss = 5000.0;
    limits.max_orders_per_minute = 100;
    limits.max_orders_per_day = 10000;
    limits.bankroll = 100000.0;
    limits.min_free_capital_pct = 0.1;

    RiskManager rm(limits);
    PositionManager pm;
    rm.set_position_manager(&pm);

    auto order = create_test_order("O1", "EUR/USD", OrderSide::BUY, 100.0, 1.1000, OrderType::LIMIT);
    auto result = rm.check_order(order, 1.1000);

    EXPECT_TRUE(result.allowed);
    EXPECT_EQ(result.reason, "OK");
}

TEST(RiskManagerTest, OrderSizeTooLarge) {
    RiskLimits limits;
    limits.max_order_size = 100.0;
    limits.max_position_size = 1000.0;
    limits.max_total_exposure = 100000.0;
    limits.bankroll = 100000.0;

    RiskManager rm(limits);
    PositionManager pm;
    rm.set_position_manager(&pm);

    auto order = create_test_order("O1", "EUR/USD", OrderSide::BUY, 150.0);
    auto result = rm.check_order(order, 1.1000);

    EXPECT_FALSE(result.allowed);
    EXPECT_NE(result.reason.find("Order size exceeds limit"), std::string::npos);
}

TEST(RiskManagerTest, PositionSizeLimit) {
    RiskLimits limits;
    limits.max_position_size = 100.0;
    limits.max_order_size = 100.0;
    limits.max_total_exposure = 100000.0;
    limits.bankroll = 100000.0;

    RiskManager rm(limits);
    PositionManager pm;
    rm.set_position_manager(&pm);

    // Create existing position
    auto fill = create_test_fill("F1", "O0", "EUR/USD", OrderSide::BUY, 80.0, 1.1000);
    pm.on_fill(fill);

    // Try to add more (80 + 50 = 130 > 100)
    auto order = create_test_order("O1", "EUR/USD", OrderSide::BUY, 50.0);
    auto result = rm.check_order(order, 1.1000);

    EXPECT_FALSE(result.allowed);
    EXPECT_NE(result.reason.find("Position size limit exceeded"), std::string::npos);
}

TEST(RiskManagerTest, InvalidQuantity) {
    RiskLimits limits;
    limits.max_position_size = 1000.0;
    limits.max_order_size = 500.0;
    limits.bankroll = 100000.0;

    RiskManager rm(limits);
    PositionManager pm;
    rm.set_position_manager(&pm);

    auto order = create_test_order("O1", "EUR/USD", OrderSide::BUY, 0.0);
    auto result = rm.check_order(order, 1.1000);

    EXPECT_FALSE(result.allowed);
    EXPECT_NE(result.reason.find("Invalid quantity"), std::string::npos);
}

TEST(RiskManagerTest, HaltOnDailyLoss) {
    RiskLimits limits;
    limits.max_position_size = 1000.0;
    limits.max_order_size = 500.0;
    limits.max_daily_loss = 100.0;
    limits.max_total_exposure = 100000.0;
    limits.bankroll = 100000.0;

    RiskManager rm(limits);
    PositionManager pm;
    rm.set_position_manager(&pm);

    // Simulate a big loss
    std::map<std::string, double> prices = {{"EUR/USD", 1.0000}};

    // Buy high, sell low to generate loss
    auto fill1 = create_test_fill("F1", "O1", "EUR/USD", OrderSide::BUY, 100.0, 1.1000);
    pm.on_fill(fill1);

    auto fill2 = create_test_fill("F2", "O2", "EUR/USD", OrderSide::SELL, 100.0, 0.9000);
    pm.on_fill(fill2);

    rm.update_market_prices(prices);

    // Next order should be rejected
    auto order = create_test_order("O3", "EUR/USD", OrderSide::BUY, 10.0);
    auto result = rm.check_order(order, 1.0000);

    EXPECT_FALSE(result.allowed);
    EXPECT_TRUE(rm.should_halt());
}

TEST(RiskManagerTest, ResetDaily) {
    RiskLimits limits;
    limits.max_position_size = 1000.0;
    limits.bankroll = 100000.0;

    RiskManager rm(limits);
    PositionManager pm;
    rm.set_position_manager(&pm);

    // Submit some orders
    auto order1 = create_test_order("O1", "EUR/USD", OrderSide::BUY, 100.0);
    rm.check_order(order1, 1.1000);

    auto metrics1 = rm.get_metrics();
    EXPECT_EQ(metrics1.orders_submitted_today, 1);

    // Reset
    rm.reset_daily();

    auto metrics2 = rm.get_metrics();
    EXPECT_EQ(metrics2.orders_submitted_today, 0);
    EXPECT_DOUBLE_EQ(metrics2.daily_pnl, 0.0);
}

// ==================== Mock Broker Tests ====================

TEST(MockBrokerTest, OrderAccepted) {
    MockBroker::Config config;
    config.fill_latency_ms = 10;
    MockBroker broker(config);

    auto order = create_test_order("O1", "EUR/USD", OrderSide::BUY, 100.0);
    auto result = broker.submit_order(order);

    EXPECT_EQ(result.status, OrderStatus::ACKNOWLEDGED);
    EXPECT_EQ(result.order_id, "O1");
}

TEST(MockBrokerTest, OrderRejection) {
    MockBroker::Config config;
    config.rejection_rate = 1.0;  // Always reject
    MockBroker broker(config);

    auto order = create_test_order("O1", "EUR/USD", OrderSide::BUY, 100.0);
    auto result = broker.submit_order(order);

    EXPECT_EQ(result.status, OrderStatus::REJECTED);
}

TEST(MockBrokerTest, FillGeneration) {
    MockBroker::Config config;
    config.fill_latency_ms = 50;
    MockBroker broker(config);
    broker.set_market_price("EUR/USD", 1.1000);

    int fill_count = 0;
    ExecutionEngine::Config engine_config;
    engine_config.enable_redis = false;

    RiskLimits limits;
    limits.max_position_size = 1000.0;
    limits.max_order_size = 500.0;
    limits.max_total_exposure = 100000.0;
    limits.bankroll = 100000.0;

    PositionManager pm;
    RiskManager rm(limits);
    rm.set_position_manager(&pm);

    ExecutionEngine engine(engine_config, &rm, &pm);
    engine.register_provider("mock", std::make_shared<MockBroker>(broker));

    engine.register_fill_callback([&fill_count](const Fill& fill) {
        fill_count++;
    });

    auto order = create_test_order("O1", "EUR/USD", OrderSide::BUY, 100.0);
    engine.submit_order(order);

    // Wait for fill
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    EXPECT_GT(fill_count, 0);
}

TEST(MockBrokerTest, CancelOrder) {
    MockBroker::Config config;
    config.fill_latency_ms = 1000;  // Long delay so we can cancel
    MockBroker broker(config);

    auto order = create_test_order("O1", "EUR/USD", OrderSide::BUY, 100.0);
    broker.submit_order(order);

    // Cancel immediately
    auto result = broker.cancel_order("O1");

    EXPECT_EQ(result.status, OrderStatus::CANCELLED);
}

// ==================== Integration Tests ====================

TEST(IntegrationTest, FullOrderLifecycle) {
    // Setup
    MockBroker::Config broker_config;
    broker_config.fill_latency_ms = 50;

    ExecutionEngine::Config engine_config;
    engine_config.enable_redis = false;

    RiskLimits limits;
    limits.max_position_size = 1000.0;
    limits.max_order_size = 500.0;
    limits.max_total_exposure = 100000.0;
    limits.max_daily_loss = 5000.0;
    limits.max_orders_per_minute = 100;
    limits.max_orders_per_day = 10000;
    limits.bankroll = 100000.0;
    limits.min_free_capital_pct = 0.1;

    PositionManager pm;
    RiskManager rm(limits);
    rm.set_position_manager(&pm);

    ExecutionEngine engine(engine_config, &rm, &pm);

    auto mock_broker = std::make_shared<MockBroker>(broker_config);
    mock_broker->set_market_price("EUR/USD", 1.1000);

    engine.register_provider("mock", mock_broker);

    // Submit order
    auto order = create_test_order("O1", "EUR/USD", OrderSide::BUY, 100.0);
    auto result = engine.submit_order(order);

    EXPECT_TRUE(result.status == OrderStatus::ACKNOWLEDGED ||
                result.status == OrderStatus::SUBMITTED);

    // Wait for fill
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Check position
    auto pos = pm.get_position("EUR/USD");
    EXPECT_NEAR(pos.quantity, 100.0, 1e-6);
}

TEST(IntegrationTest, RiskRejection) {
    MockBroker::Config broker_config;
    ExecutionEngine::Config engine_config;
    engine_config.enable_redis = false;

    RiskLimits limits;
    limits.max_position_size = 50.0;  // Small limit
    limits.max_order_size = 100.0;
    limits.max_total_exposure = 100000.0;
    limits.bankroll = 100000.0;

    PositionManager pm;
    RiskManager rm(limits);
    rm.set_position_manager(&pm);

    ExecutionEngine engine(engine_config, &rm, &pm);
    engine.register_provider("mock", std::make_shared<MockBroker>(broker_config));

    // This should be rejected by risk manager
    auto order = create_test_order("O1", "EUR/USD", OrderSide::BUY, 100.0);
    auto result = engine.submit_order(order);

    EXPECT_EQ(result.status, OrderStatus::REJECTED);
    EXPECT_NE(result.reason.find("Risk check failed"), std::string::npos);
}

TEST(IntegrationTest, PartialFills) {
    MockBroker::Config broker_config;
    broker_config.fill_latency_ms = 50;
    broker_config.enable_partial_fills = true;
    broker_config.partial_fill_count = 3;

    ExecutionEngine::Config engine_config;
    engine_config.enable_redis = false;

    RiskLimits limits;
    limits.max_position_size = 1000.0;
    limits.max_order_size = 500.0;
    limits.max_total_exposure = 100000.0;
    limits.bankroll = 100000.0;

    PositionManager pm;
    RiskManager rm(limits);
    rm.set_position_manager(&pm);

    ExecutionEngine engine(engine_config, &rm, &pm);

    auto mock_broker = std::make_shared<MockBroker>(broker_config);
    mock_broker->set_market_price("EUR/USD", 1.1000);
    engine.register_provider("mock", mock_broker);

    int fill_count = 0;
    engine.register_fill_callback([&fill_count](const Fill& fill) {
        fill_count++;
    });

    auto order = create_test_order("O1", "EUR/USD", OrderSide::BUY, 300.0);
    engine.submit_order(order);

    // Wait for all fills
    std::this_thread::sleep_for(std::chrono::milliseconds(300));

    EXPECT_EQ(fill_count, 3);  // 3 partial fills

    auto pos = pm.get_position("EUR/USD");
    EXPECT_NEAR(pos.quantity, 300.0, 1e-6);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
