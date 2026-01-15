/**
 * Opening Range Breakout (ORB) Analyzer
 * Statistical analysis of opening range breakouts
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>

namespace quantumliquidity::analytics {

struct ORBStats {
    std::string instrument;
    int period_minutes;          // Opening range period (e.g., 30 min)

    // Opening range metrics
    double or_high;
    double or_low;
    double or_range;
    double or_midpoint;

    // Day metrics
    double day_high;
    double day_low;
    double day_close;
    double day_range;

    // Breakout analysis
    bool broke_high;
    bool broke_low;
    double breakout_extension;   // How far beyond OR
    double breakout_time_mins;   // When breakout occurred

    // Statistical metrics
    double or_to_day_ratio;      // OR range / Day range
    double efficiency_ratio;     // Net move / Total range

    int64_t date_timestamp_ns;
};

struct ORBSummary {
    std::string instrument;
    int period_minutes;
    int total_days;

    // Breakout statistics
    int high_breakouts;
    int low_breakouts;
    double high_breakout_pct;
    double low_breakout_pct;

    // Average metrics
    double avg_or_range;
    double avg_day_range;
    double avg_or_to_day_ratio;
    double avg_breakout_extension;

    // Profitability (simulated)
    double total_pnl;
    double win_rate;
    double profit_factor;
};

class ORBAnalyzer {
public:
    ORBAnalyzer(int period_minutes = 30);

    /**
     * Analyze single day for ORB statistics
     */
    ORBStats analyze_day(
        const std::string& instrument,
        const std::vector<std::tuple<double, double, double, double, int64_t>>& bars,
        int64_t session_start_ns
    );

    /**
     * Generate summary statistics over multiple days
     */
    ORBSummary summarize(
        const std::string& instrument,
        const std::vector<ORBStats>& daily_stats
    );

    /**
     * Get current period in minutes
     */
    int get_period_minutes() const { return period_minutes_; }

private:
    int period_minutes_;

    // Helper functions
    std::pair<double, double> calculate_opening_range(
        const std::vector<std::tuple<double, double, double, double, int64_t>>& bars,
        int64_t session_start_ns,
        int64_t period_end_ns
    );

    double calculate_breakout_time(
        const std::vector<std::tuple<double, double, double, double, int64_t>>& bars,
        int64_t period_end_ns,
        double threshold_price,
        bool looking_for_high
    );
};

} // namespace quantumliquidity::analytics
