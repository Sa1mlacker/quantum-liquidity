#pragma once

#include <string>
#include <cstdint>
#include <optional>
#include <chrono>

namespace quantumliquidity::execution {

/**
 * @brief Order side (direction)
 */
enum class OrderSide { 
    BUY = 1, 
    SELL = -1 
};

/**
 * @brief Order type
 */
enum class OrderType { 
    MARKET,     // Execute at current market price
    LIMIT,      // Execute at specified price or better
    STOP,       // Trigger when price reaches stop level
    STOP_LIMIT  // Combination of stop and limit
};

/**
 * @brief Time in force
 */
enum class TimeInForce {
    DAY,        // Valid for trading day
    GTC,        // Good till cancelled
    IOC,        // Immediate or cancel
    FOK         // Fill or kill
};

/**
 * @brief Order status
 */
enum class OrderStatus {
    PENDING,            // Created but not sent yet
    SUBMITTED,          // Sent to broker
    ACKNOWLEDGED,       // Broker confirmed receipt
    PARTIALLY_FILLED,   // Partially executed
    FILLED,             // Fully executed
    CANCELLED,          // Successfully cancelled
    REJECTED,           // Rejected by broker or risk check
    ERROR,              // Error during processing
    EXPIRED             // Expired (e.g., DAY order after close)
};

/**
 * @brief Order request from strategy
 */
struct OrderRequest {
    std::string order_id;           // UUID or sequential ID
    std::string instrument;         // EUR/USD, AAPL, etc.
    OrderSide side;                 // BUY or SELL
    OrderType type;                 // MARKET, LIMIT, etc.
    double quantity;                // Number of contracts/shares
    double price;                   // Limit price (0 for MARKET)
    TimeInForce tif;                // Time in force
    std::string strategy_id;        // Which strategy placed this order
    std::string user_comment;       // Optional comment
    int64_t timestamp_ns;           // When created (nanoseconds)
    
    // Optional stop price for STOP orders
    std::optional<double> stop_price;
};

/**
 * @brief Order update from broker/exchange
 */
struct OrderUpdate {
    std::string order_id;           // Links to OrderRequest
    OrderStatus status;             // Current status
    double filled_qty;              // Cumulative filled quantity
    double remaining_qty;           // Quantity still open
    double avg_fill_price;          // Average price of fills
    std::string reason;             // If REJECTED or ERROR
    int64_t timestamp_ns;           // When update received
    
    // Optional exchange order ID
    std::optional<std::string> exchange_order_id;
};

/**
 * @brief Fill (trade execution)
 */
struct Fill {
    std::string fill_id;            // Unique fill ID
    std::string order_id;           // Parent order
    std::string instrument;         // What was traded
    OrderSide side;                 // BUY or SELL
    double quantity;                // Executed quantity
    double price;                   // Execution price
    double commission;              // Broker commission
    int64_t timestamp_ns;           // When filled
    
    // Optional exchange trade ID
    std::optional<std::string> exchange_trade_id;
};

/**
 * @brief Position (current holdings)
 */
struct Position {
    std::string instrument;         // What instrument
    double quantity;                // Signed: + = long, - = short
    double entry_price;             // Weighted average entry price
    double unrealized_pnl;          // Mark-to-market PnL
    double realized_pnl;            // Closed PnL today
    int64_t last_update_ns;         // Last modification time
    
    // Additional tracking
    int num_fills_today;            // How many fills contributed
    double total_commission;        // Total commission paid
};

/**
 * @brief Order modification request
 */
struct OrderModification {
    std::string order_id;           // Which order to modify
    std::optional<double> new_price;        // New limit price
    std::optional<double> new_quantity;     // New quantity
    std::optional<double> new_stop_price;   // New stop price
    int64_t timestamp_ns;           // When requested
};

/**
 * @brief Risk metrics snapshot
 */
struct RiskMetrics {
    // Exposure
    double total_exposure;          // Sum of |qty * price| across all positions
    double account_utilization;     // % of bankroll used
    double max_position_exposure;   // Largest single position (absolute $)
    
    // PnL
    double daily_pnl;               // Total PnL today (realized + unrealized)
    double realized_pnl;            // Only closed trades
    double unrealized_pnl;          // Open positions mark-to-market
    double max_dd_today;            // Max drawdown from daily high
    double daily_high_pnl;          // Highest PnL reached today
    
    // Order stats
    int orders_submitted_today;     // Total orders sent
    int orders_filled_today;        // Successfully filled
    int orders_rejected_today;      // Rejected by risk or broker
    int orders_cancelled_today;     // Cancelled by user
    
    // Risk limits status
    bool halt_active;               // Is trading halted?
    std::string halt_reason;        // Why halted (if halt_active)
    
    int64_t timestamp_ns;           // When snapshot taken
};

/**
 * @brief Risk check result
 */
struct RiskCheckResult {
    bool allowed;                   // Can this order proceed?
    std::string reason;             // "OK" or rejection reason
    
    // Additional info
    double reserved_capital;        // How much capital this order reserves
    double new_exposure;            // Total exposure if order fills
    double new_position_size;       // New position size if order fills
};

// Helper functions

inline const char* order_side_to_string(OrderSide side) {
    return side == OrderSide::BUY ? "BUY" : "SELL";
}

inline const char* order_type_to_string(OrderType type) {
    switch (type) {
        case OrderType::MARKET: return "MARKET";
        case OrderType::LIMIT: return "LIMIT";
        case OrderType::STOP: return "STOP";
        case OrderType::STOP_LIMIT: return "STOP_LIMIT";
        default: return "UNKNOWN";
    }
}

inline const char* order_status_to_string(OrderStatus status) {
    switch (status) {
        case OrderStatus::PENDING: return "PENDING";
        case OrderStatus::SUBMITTED: return "SUBMITTED";
        case OrderStatus::ACKNOWLEDGED: return "ACKNOWLEDGED";
        case OrderStatus::PARTIALLY_FILLED: return "PARTIALLY_FILLED";
        case OrderStatus::FILLED: return "FILLED";
        case OrderStatus::CANCELLED: return "CANCELLED";
        case OrderStatus::REJECTED: return "REJECTED";
        case OrderStatus::ERROR: return "ERROR";
        case OrderStatus::EXPIRED: return "EXPIRED";
        default: return "UNKNOWN";
    }
}

inline const char* tif_to_string(TimeInForce tif) {
    switch (tif) {
        case TimeInForce::DAY: return "DAY";
        case TimeInForce::GTC: return "GTC";
        case TimeInForce::IOC: return "IOC";
        case TimeInForce::FOK: return "FOK";
        default: return "UNKNOWN";
    }
}

} // namespace quantumliquidity::execution
