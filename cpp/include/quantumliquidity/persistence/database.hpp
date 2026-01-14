#pragma once

#include "quantumliquidity/common/types.hpp"
#include <memory>
#include <string>
#include <vector>
#include <functional>

namespace quantumliquidity::persistence {

/**
 * @brief Database connection configuration
 */
struct DatabaseConfig {
    std::string host = "localhost";
    int port = 5432;
    std::string database = "quantumliquidity";
    std::string user = "quantumliquidity";
    std::string password;
    int pool_size = 10;
    int max_overflow = 20;
    int connection_timeout_ms = 5000;
};

/**
 * @brief Database connection interface
 */
class IConnection {
public:
    virtual ~IConnection() = default;

    // Execute query without results
    virtual void execute(const std::string& query) = 0;

    // Execute query with parameters
    virtual void execute_params(
        const std::string& query,
        const std::vector<std::string>& params
    ) = 0;

    // Query with results (callback per row)
    using RowCallback = std::function<void(const std::vector<std::string>& row)>;
    virtual void query(const std::string& query, RowCallback callback) = 0;

    // Begin/commit/rollback transaction
    virtual void begin_transaction() = 0;
    virtual void commit() = 0;
    virtual void rollback() = 0;

    // Check if connection is alive
    virtual bool is_alive() const = 0;
};

/**
 * @brief Connection pool interface
 */
class IConnectionPool {
public:
    virtual ~IConnectionPool() = default;

    // Acquire connection from pool (blocks if none available)
    virtual std::shared_ptr<IConnection> acquire() = 0;

    // Release connection back to pool (automatic via shared_ptr)
    virtual void release(std::shared_ptr<IConnection> conn) = 0;

    // Pool statistics
    struct Stats {
        int total_connections;
        int available_connections;
        int active_connections;
        uint64_t total_acquires;
        uint64_t total_releases;
    };

    virtual Stats get_stats() const = 0;

    // Lifecycle
    virtual void start() = 0;
    virtual void stop() = 0;
};

/**
 * @brief Batch writer for time-series data
 */
class ITimeSeriesWriter {
public:
    virtual ~ITimeSeriesWriter() = default;

    // Write ticks
    virtual void write_tick(const Tick& tick) = 0;
    virtual void write_ticks(const std::vector<Tick>& ticks) = 0;

    // Write bars
    virtual void write_bar(const Bar& bar) = 0;
    virtual void write_bars(const std::vector<Bar>& bars) = 0;

    // Write orders/trades
    virtual void write_order(const OrderUpdate& order) = 0;
    virtual void write_fill(const Fill& fill) = 0;

    // Flush pending writes
    virtual void flush() = 0;

    // Statistics
    struct Stats {
        uint64_t ticks_written;
        uint64_t bars_written;
        uint64_t orders_written;
        uint64_t fills_written;
        uint64_t flush_count;
        uint64_t error_count;
    };

    virtual Stats get_stats() const = 0;
};

/**
 * @brief Time-series reader for historical data
 */
class ITimeSeriesReader {
public:
    virtual ~ITimeSeriesReader() = default;

    // Read ticks
    virtual std::vector<Tick> read_ticks(
        const InstrumentID& instrument,
        Timestamp start,
        Timestamp end
    ) = 0;

    // Read bars
    virtual std::vector<Bar> read_bars(
        const InstrumentID& instrument,
        TimeFrame timeframe,
        Timestamp start,
        Timestamp end
    ) = 0;
};

} // namespace quantumliquidity::persistence
