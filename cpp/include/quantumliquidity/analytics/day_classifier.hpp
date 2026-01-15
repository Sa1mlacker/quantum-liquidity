/**
 * Day Classification Engine
 * Classifies trading days into: Trend, Range, V-Day, P-Day
 * Based on price action patterns and volatility
 */

#pragma once

#include <string>
#include <vector>
#include <memory>

namespace quantumliquidity::analytics {

enum class DayType {
    TREND_UP,      // Strong upward momentum
    TREND_DOWN,    // Strong downward momentum
    RANGE,         // Sideways consolidation
    V_DAY,         // V-shaped reversal
    P_DAY,         // Progressive breakout
    UNDEFINED      // Not enough data
};

struct DayStats {
    DayType type;
    double open;
    double high;
    double low;
    double close;
    double range;
    double body_pct;          // Body as % of range
    double wick_top_pct;      // Upper wick as % of range
    double wick_bottom_pct;   // Lower wick as % of range
    double volatility;        // Intraday volatility
    int64_t timestamp_ns;

    std::string type_str() const;
};

class DayClassifier {
public:
    DayClassifier();

    /**
     * Classify a day based on OHLC data
     */
    DayStats classify(
        double open,
        double high,
        double low,
        double close,
        int64_t timestamp_ns
    );

    /**
     * Classify using bar data
     */
    DayStats classify_from_bars(
        const std::vector<std::tuple<double, double, double, double, int64_t>>& bars
    );

    /**
     * Get classification confidence (0.0 - 1.0)
     */
    double get_confidence() const { return confidence_; }

private:
    double confidence_;

    // Classification thresholds
    static constexpr double TREND_THRESHOLD = 0.7;    // Body must be >70% of range
    static constexpr double RANGE_THRESHOLD = 0.4;    // Body must be <40% of range
    static constexpr double V_DAY_THRESHOLD = 0.3;    // Both wicks >30% of range
    static constexpr double P_DAY_THRESHOLD = 0.6;    // Progressive move >60%

    DayType determine_type(const DayStats& stats);
    double calculate_confidence(const DayStats& stats, DayType type);
};

} // namespace quantumliquidity::analytics
