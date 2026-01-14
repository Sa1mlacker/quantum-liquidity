#pragma once

#include "quantumliquidity/execution/types.hpp"
#include "quantumliquidity/persistence/database.hpp"
#include <map>
#include <mutex>
#include <memory>

namespace quantumliquidity::execution {

/**
 * @brief Position Manager - tracks all open positions and PnL
 *
 * Thread-safe position tracking with:
 * - Real-time PnL calculation (realized + unrealized)
 * - Weighted average entry price calculation
 * - Position lifecycle management (open, increase, decrease, close)
 * - PostgreSQL persistence
 */
class PositionManager {
public:
    PositionManager();
    ~PositionManager();

    /**
     * @brief Process fill and update position
     *
     * Updates position quantity, entry price (weighted average),
     * and calculates realized PnL if position is reduced/closed.
     *
     * @param fill Executed fill
     */
    void on_fill(const Fill& fill);

    /**
     * @brief Get current position for instrument
     *
     * @param instrument Instrument ID
     * @return Position (zero if no position exists)
     */
    Position get_position(const std::string& instrument) const;

    /**
     * @brief Get all current positions
     *
     * @return Map of instrument to position
     */
    std::map<std::string, Position> get_all_positions() const;

    /**
     * @brief Get unrealized PnL for specific position
     *
     * Calculates mark-to-market PnL based on current market price.
     *
     * @param instrument Instrument ID
     * @param current_price Current market price
     * @return Unrealized PnL (0 if no position)
     */
    double get_unrealized_pnl(const std::string& instrument, double current_price) const;

    /**
     * @brief Get total unrealized PnL across all positions
     *
     * @param current_prices Map of instrument to current price
     * @return Total unrealized PnL
     */
    double get_total_unrealized_pnl(const std::map<std::string, double>& current_prices) const;

    /**
     * @brief Get total realized PnL (closed trades only)
     *
     * @return Total realized PnL today
     */
    double get_total_realized_pnl() const;

    /**
     * @brief Get total exposure (sum of |qty * price|)
     *
     * @param current_prices Map of instrument to current price
     * @return Total exposure in $
     */
    double get_total_exposure(const std::map<std::string, double>& current_prices) const;

    /**
     * @brief Check if position exists for instrument
     *
     * @param instrument Instrument ID
     * @return true if position exists (qty != 0)
     */
    bool has_position(const std::string& instrument) const;

    /**
     * @brief Get current quantity for instrument
     *
     * @param instrument Instrument ID
     * @return Signed quantity (+ = long, - = short, 0 = flat)
     */
    double get_quantity(const std::string& instrument) const;

    /**
     * @brief Persist positions to PostgreSQL
     *
     * Writes current positions to database for historical tracking.
     *
     * @param writer Time series writer
     */
    void persist_positions(persistence::ITimeSeriesWriter* writer);

    /**
     * @brief Reset daily PnL
     *
     * Call at start of trading day. Resets realized_pnl but keeps positions.
     */
    void reset_daily();

    /**
     * @brief Get position statistics
     */
    struct Stats {
        int num_positions;              // Number of open positions
        double total_realized_pnl;      // Total realized PnL
        double total_unrealized_pnl;    // Total unrealized PnL (requires prices)
        double total_commission_paid;   // Total commission across all positions
        int total_fills_today;          // Total fills processed today
    };

    Stats get_stats(const std::map<std::string, double>& current_prices) const;

private:
    // Helper: calculate new weighted average entry price
    double calculate_weighted_avg_price(
        double current_qty,
        double current_entry,
        double fill_qty,
        double fill_price
    ) const;
    
    // Helper: calculate realized PnL when reducing position
    double calculate_realized_pnl(
        double position_qty,
        double entry_price,
        double fill_qty,
        double fill_price
    ) const;

    std::map<std::string, Position> positions_;
    double total_realized_pnl_;
    int total_fills_today_;
    
    mutable std::mutex mutex_;
};

} // namespace quantumliquidity::execution
