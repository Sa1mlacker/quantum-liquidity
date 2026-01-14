#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <optional>

namespace quantumliquidity {

// Type aliases for clarity
using Timestamp = std::chrono::system_clock::time_point;
using Duration = std::chrono::microseconds;
using InstrumentID = std::string;
using OrderID = uint64_t;
using StrategyID = std::string;
using Price = double;
using Quantity = double;

// Enum: Time frames
enum class TimeFrame : uint8_t {
    TICK = 0,
    SEC_1 = 1,
    SEC_5 = 5,
    SEC_15 = 15,
    SEC_30 = 30,
    MIN_1 = 60,
    MIN_5 = 300,
    MIN_15 = 900,
    MIN_30 = 1800,
    HOUR_1 = 3600,
    HOUR_4 = 14400,
    DAY_1 = 86400
};

// Enum: Order side
enum class Side : uint8_t {
    BUY,
    SELL
};

// Enum: Order type
enum class OrderType : uint8_t {
    MARKET,
    LIMIT,
    STOP,
    STOP_LIMIT
};

// Enum: Order state
enum class OrderState : uint8_t {
    PENDING_SUBMIT,
    SUBMITTED,
    ACCEPTED,
    PARTIALLY_FILLED,
    FILLED,
    PENDING_CANCEL,
    CANCELLED,
    REJECTED,
    EXPIRED
};

// Enum: Time in force
enum class TimeInForce : uint8_t {
    GTC,  // Good till cancel
    IOC,  // Immediate or cancel
    FOK,  // Fill or kill
    DAY   // Day order
};

// Struct: Tick data
struct Tick {
    Timestamp timestamp;
    InstrumentID instrument;
    Price bid;
    Price ask;
    Quantity bid_size;
    Quantity ask_size;
    std::optional<Quantity> last_trade_size;
    std::optional<Price> last_trade_price;
};

// Struct: OHLCV bar
struct Bar {
    Timestamp timestamp;
    InstrumentID instrument;
    TimeFrame timeframe;
    Price open;
    Price high;
    Price low;
    Price close;
    Quantity volume;
    int32_t tick_count;
};

// Struct: Depth update (order book)
struct DepthLevel {
    Price price;
    Quantity size;
};

struct DepthUpdate {
    Timestamp timestamp;
    InstrumentID instrument;
    std::vector<DepthLevel> bids;  // Sorted descending
    std::vector<DepthLevel> asks;  // Sorted ascending
};

// Struct: Order request
struct OrderRequest {
    StrategyID strategy_id;
    InstrumentID instrument;
    Side side;
    OrderType type;
    Quantity quantity;
    std::optional<Price> limit_price;
    std::optional<Price> stop_price;
    TimeInForce time_in_force = TimeInForce::GTC;
    std::string client_order_id;
};

// Struct: Order update (feedback from exchange)
struct OrderUpdate {
    OrderID order_id;
    std::string client_order_id;
    OrderState state;
    Timestamp timestamp;
    std::optional<std::string> reject_reason;
    Quantity filled_quantity;
    Quantity remaining_quantity;
    std::optional<Price> average_fill_price;
};

// Struct: Fill (execution)
struct Fill {
    OrderID order_id;
    Timestamp timestamp;
    InstrumentID instrument;
    Side side;
    Quantity quantity;
    Price price;
    std::optional<std::string> execution_id;
    std::optional<double> commission;
};

// Struct: Position
struct Position {
    InstrumentID instrument;
    Quantity quantity;  // Positive = long, negative = short
    Price average_price;
    double unrealized_pnl;
    double realized_pnl;
    Timestamp last_update;
};

// Struct: Strategy state
enum class StrategyState : uint8_t {
    INACTIVE,
    STARTING,
    RUNNING,
    STOPPING,
    STOPPED,
    ERROR
};

// Struct: Risk metrics
struct RiskMetrics {
    double total_exposure;       // Sum of position values
    double total_unrealized_pnl;
    double total_realized_pnl;
    double daily_pnl;
    double max_drawdown;
    uint32_t order_count_last_minute;
    Timestamp last_update;
};

// Struct: Risk check result
struct RiskCheckResult {
    bool passed;
    std::string reason;  // Empty if passed
};

// Instrument metadata
struct InstrumentInfo {
    InstrumentID id;
    std::string symbol;
    std::string asset_class;  // FX, METAL, INDEX
    double min_price_increment;
    double min_quantity;
    double contract_size;
    std::string base_currency;
    std::string quote_currency;
};

} // namespace quantumliquidity
