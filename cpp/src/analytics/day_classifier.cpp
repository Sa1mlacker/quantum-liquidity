/**
 * Day Classification Implementation
 */

#include "quantumliquidity/analytics/day_classifier.hpp"
#include <cmath>
#include <algorithm>

namespace quantumliquidity::analytics {

std::string DayStats::type_str() const {
    switch (type) {
        case DayType::TREND_UP: return "TREND_UP";
        case DayType::TREND_DOWN: return "TREND_DOWN";
        case DayType::RANGE: return "RANGE";
        case DayType::V_DAY: return "V_DAY";
        case DayType::P_DAY: return "P_DAY";
        default: return "UNDEFINED";
    }
}

DayClassifier::DayClassifier() : confidence_(0.0) {}

DayStats DayClassifier::classify(
    double open,
    double high,
    double low,
    double close,
    int64_t timestamp_ns
) {
    DayStats stats;
    stats.open = open;
    stats.high = high;
    stats.low = low;
    stats.close = close;
    stats.timestamp_ns = timestamp_ns;

    // Calculate range
    stats.range = high - low;
    if (stats.range < 1e-8) {
        stats.type = DayType::UNDEFINED;
        confidence_ = 0.0;
        return stats;
    }

    // Calculate body and wicks as percentage of range
    double body = std::abs(close - open);
    double upper_wick = high - std::max(open, close);
    double lower_wick = std::min(open, close) - low;

    stats.body_pct = body / stats.range;
    stats.wick_top_pct = upper_wick / stats.range;
    stats.wick_bottom_pct = lower_wick / stats.range;

    // Calculate volatility (simplified)
    stats.volatility = stats.range / open;

    // Determine day type
    stats.type = determine_type(stats);
    confidence_ = calculate_confidence(stats, stats.type);

    return stats;
}

DayStats DayClassifier::classify_from_bars(
    const std::vector<std::tuple<double, double, double, double, int64_t>>& bars
) {
    if (bars.empty()) {
        DayStats stats;
        stats.type = DayType::UNDEFINED;
        confidence_ = 0.0;
        return stats;
    }

    // Get session OHLC
    double open = std::get<0>(bars.front());
    double high = open;
    double low = open;
    double close = std::get<3>(bars.back());
    int64_t timestamp = std::get<4>(bars.back());

    for (const auto& bar : bars) {
        high = std::max(high, std::get<1>(bar));
        low = std::min(low, std::get<2>(bar));
    }

    return classify(open, high, low, close, timestamp);
}

DayType DayClassifier::determine_type(const DayStats& stats) {
    // V-Day: Large wicks on both sides (reversal pattern)
    if (stats.wick_top_pct > V_DAY_THRESHOLD &&
        stats.wick_bottom_pct > V_DAY_THRESHOLD) {
        return DayType::V_DAY;
    }

    // Trend: Large body, small wicks
    if (stats.body_pct > TREND_THRESHOLD) {
        return (stats.close > stats.open) ? DayType::TREND_UP : DayType::TREND_DOWN;
    }

    // Range: Small body relative to range
    if (stats.body_pct < RANGE_THRESHOLD) {
        return DayType::RANGE;
    }

    // P-Day: Progressive move with one dominant wick
    if (stats.body_pct > P_DAY_THRESHOLD) {
        if (stats.close > stats.open && stats.wick_bottom_pct < 0.15) {
            return DayType::P_DAY;
        }
        if (stats.close < stats.open && stats.wick_top_pct < 0.15) {
            return DayType::P_DAY;
        }
    }

    return DayType::UNDEFINED;
}

double DayClassifier::calculate_confidence(const DayStats& stats, DayType type) {
    switch (type) {
        case DayType::TREND_UP:
        case DayType::TREND_DOWN:
            // Confidence based on body percentage
            return std::min(1.0, stats.body_pct / TREND_THRESHOLD);

        case DayType::RANGE:
            // Confidence inversely related to body size
            return std::min(1.0, 1.0 - (stats.body_pct / RANGE_THRESHOLD));

        case DayType::V_DAY:
            // Confidence based on wick symmetry
            return std::min(1.0, (stats.wick_top_pct + stats.wick_bottom_pct) / (2 * V_DAY_THRESHOLD));

        case DayType::P_DAY:
            // Confidence based on body dominance
            return std::min(1.0, stats.body_pct / P_DAY_THRESHOLD);

        default:
            return 0.0;
    }
}

} // namespace quantumliquidity::analytics
