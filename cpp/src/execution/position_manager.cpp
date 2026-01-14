#include "quantumliquidity/execution/position_manager.hpp"
#include "quantumliquidity/common/logger.hpp"
#include <cmath>
#include <sstream>

namespace quantumliquidity::execution {

PositionManager::PositionManager()
    : total_realized_pnl_(0.0)
    , total_fills_today_(0)
{
    Logger::info("execution", "Position manager initialized");
}

PositionManager::~PositionManager() {
    Logger::info("execution", "Position manager shutdown");
}

void PositionManager::on_fill(const Fill& fill) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    total_fills_today_++;
    
    auto it = positions_.find(fill.instrument);
    
    // Calculate fill quantity with sign
    double signed_fill_qty = (fill.side == OrderSide::BUY ? 1.0 : -1.0) * fill.quantity;
    
    if (it == positions_.end()) {
        // New position
        Position pos;
        pos.instrument = fill.instrument;
        pos.quantity = signed_fill_qty;
        pos.entry_price = fill.price;
        pos.unrealized_pnl = 0.0;
        pos.realized_pnl = 0.0;
        pos.last_update_ns = fill.timestamp_ns;
        pos.num_fills_today = 1;
        pos.total_commission = fill.commission;
        
        positions_[fill.instrument] = pos;
        
        Logger::info("execution", 
            "New position opened: instrument={}, qty={:.2f}, entry_price={:.5f}", 
            fill.instrument, pos.quantity, pos.entry_price);
            
    } else {
        Position& pos = it->second;
        
        // Check if this is increasing, decreasing, or reversing position
        bool same_direction = (pos.quantity * signed_fill_qty) > 0;
        
        if (same_direction || pos.quantity == 0) {
            // Increasing position or opening from flat
            pos.entry_price = calculate_weighted_avg_price(
                pos.quantity, pos.entry_price,
                signed_fill_qty, fill.price
            );
            pos.quantity += signed_fill_qty;
            
            Logger::info("execution",
                "Position increased: instrument={}, new_qty={:.2f}, new_entry={:.5f}",
                fill.instrument, pos.quantity, pos.entry_price);
                
        } else {
            // Decreasing or reversing position - realize PnL
            double realized = calculate_realized_pnl(
                pos.quantity, pos.entry_price,
                signed_fill_qty, fill.price
            );
            
            pos.realized_pnl += realized;
            total_realized_pnl_ += realized;
            
            double old_qty = pos.quantity;
            pos.quantity += signed_fill_qty;
            
            // If position reversed (crossed zero), update entry price
            if ((old_qty > 0 && pos.quantity < 0) || (old_qty < 0 && pos.quantity > 0)) {
                pos.entry_price = fill.price;
                Logger::info("execution",
                    "Position reversed: instrument={}, new_qty={:.2f}, realized_pnl={:.2f}",
                    fill.instrument, pos.quantity, realized);
            } else if (std::abs(pos.quantity) < 1e-8) {
                // Position closed
                Logger::info("execution",
                    "Position closed: instrument={}, realized_pnl={:.2f}",
                    fill.instrument, realized);
            } else {
                // Position reduced but not closed
                Logger::info("execution",
                    "Position reduced: instrument={}, new_qty={:.2f}, realized_pnl={:.2f}",
                    fill.instrument, pos.quantity, realized);
            }
        }
        
        pos.last_update_ns = fill.timestamp_ns;
        pos.num_fills_today++;
        pos.total_commission += fill.commission;
    }
}

Position PositionManager::get_position(const std::string& instrument) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = positions_.find(instrument);
    if (it != positions_.end()) {
        return it->second;
    }
    
    // Return empty position
    Position empty;
    empty.instrument = instrument;
    empty.quantity = 0.0;
    empty.entry_price = 0.0;
    empty.unrealized_pnl = 0.0;
    empty.realized_pnl = 0.0;
    empty.last_update_ns = 0;
    empty.num_fills_today = 0;
    empty.total_commission = 0.0;
    
    return empty;
}

std::map<std::string, Position> PositionManager::get_all_positions() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return positions_;
}

double PositionManager::get_unrealized_pnl(const std::string& instrument, double current_price) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = positions_.find(instrument);
    if (it == positions_.end() || std::abs(it->second.quantity) < 1e-8) {
        return 0.0;
    }
    
    const Position& pos = it->second;
    
    // PnL = quantity * (current_price - entry_price)
    // Positive quantity (long): profit if price goes up
    // Negative quantity (short): profit if price goes down
    return pos.quantity * (current_price - pos.entry_price);
}

double PositionManager::get_total_unrealized_pnl(const std::map<std::string, double>& current_prices) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    double total_unrealized = 0.0;
    
    for (const auto& [instrument, pos] : positions_) {
        if (std::abs(pos.quantity) < 1e-8) continue;
        
        auto price_it = current_prices.find(instrument);
        if (price_it != current_prices.end()) {
            total_unrealized += pos.quantity * (price_it->second - pos.entry_price);
        }
    }
    
    return total_unrealized;
}

double PositionManager::get_total_realized_pnl() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return total_realized_pnl_;
}

double PositionManager::get_total_exposure(const std::map<std::string, double>& current_prices) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    double total_exposure = 0.0;
    
    for (const auto& [instrument, pos] : positions_) {
        if (std::abs(pos.quantity) < 1e-8) continue;
        
        auto price_it = current_prices.find(instrument);
        if (price_it != current_prices.end()) {
            total_exposure += std::abs(pos.quantity * price_it->second);
        }
    }
    
    return total_exposure;
}

bool PositionManager::has_position(const std::string& instrument) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = positions_.find(instrument);
    return it != positions_.end() && std::abs(it->second.quantity) >= 1e-8;
}

double PositionManager::get_quantity(const std::string& instrument) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = positions_.find(instrument);
    if (it != positions_.end()) {
        return it->second.quantity;
    }
    return 0.0;
}

void PositionManager::persist_positions(persistence::ITimeSeriesWriter* writer) {
    if (!writer) {
        Logger::warning("execution", "No writer provided for position persistence");
        return;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // TODO: Implement when ITimeSeriesWriter has write_position method
    Logger::debug("execution", "Persisting {} positions to database", positions_.size());
}

void PositionManager::reset_daily() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Reset daily counters but keep positions
    total_realized_pnl_ = 0.0;
    total_fills_today_ = 0;
    
    for (auto& [instrument, pos] : positions_) {
        pos.realized_pnl = 0.0;
        pos.num_fills_today = 0;
        pos.total_commission = 0.0;
    }
    
    Logger::info("execution", "Daily position counters reset");
}

PositionManager::Stats PositionManager::get_stats(const std::map<std::string, double>& current_prices) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    Stats stats;
    stats.num_positions = 0;
    stats.total_realized_pnl = total_realized_pnl_;
    stats.total_unrealized_pnl = 0.0;
    stats.total_commission_paid = 0.0;
    stats.total_fills_today = total_fills_today_;
    
    for (const auto& [instrument, pos] : positions_) {
        if (std::abs(pos.quantity) >= 1e-8) {
            stats.num_positions++;
        }
        
        stats.total_commission_paid += pos.total_commission;
        
        auto price_it = current_prices.find(instrument);
        if (price_it != current_prices.end()) {
            stats.total_unrealized_pnl += pos.quantity * (price_it->second - pos.entry_price);
        }
    }
    
    return stats;
}

double PositionManager::calculate_weighted_avg_price(
    double current_qty,
    double current_entry,
    double fill_qty,
    double fill_price
) const {
    if (std::abs(current_qty + fill_qty) < 1e-8) {
        return 0.0;  // Position will be zero
    }
    
    // Weighted average: (qty1 * price1 + qty2 * price2) / (qty1 + qty2)
    double total_cost = current_qty * current_entry + fill_qty * fill_price;
    double total_qty = current_qty + fill_qty;
    
    return total_cost / total_qty;
}

double PositionManager::calculate_realized_pnl(
    double position_qty,
    double entry_price,
    double fill_qty,
    double fill_price
) const {
    // When reducing position, realize PnL on the closed portion
    // PnL = qty_closed * (exit_price - entry_price)
    
    double qty_to_close = std::min(std::abs(position_qty), std::abs(fill_qty));
    
    // If position is long and we're selling, or short and buying
    bool reducing = (position_qty * fill_qty) < 0;
    
    if (!reducing) {
        return 0.0;  // Not closing any position
    }
    
    // Calculate PnL with correct sign
    if (position_qty > 0) {
        // Long position, selling
        return qty_to_close * (fill_price - entry_price);
    } else {
        // Short position, buying back
        return qty_to_close * (entry_price - fill_price);
    }
}

} // namespace quantumliquidity::execution
