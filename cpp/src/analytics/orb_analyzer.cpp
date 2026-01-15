/**
 * ORB Analyzer Implementation
 */

#include "quantumliquidity/analytics/orb_analyzer.hpp"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace quantumliquidity::analytics {

ORBAnalyzer::ORBAnalyzer(int period_minutes) : period_minutes_(period_minutes) {}

ORBStats ORBAnalyzer::analyze_day(
    const std::string& instrument,
    const std::vector<std::tuple<double, double, double, double, int64_t>>& bars,
    int64_t session_start_ns
) {
    ORBStats stats;
    stats.instrument = instrument;
    stats.period_minutes = period_minutes_;
    stats.date_timestamp_ns = session_start_ns;

    if (bars.empty()) {
        return stats;
    }

    // Calculate period end time
    int64_t period_duration_ns = static_cast<int64_t>(period_minutes_) * 60 * 1000000000LL;
    int64_t period_end_ns = session_start_ns + period_duration_ns;

    // Get opening range
    auto [or_high, or_low] = calculate_opening_range(bars, session_start_ns, period_end_ns);
    stats.or_high = or_high;
    stats.or_low = or_low;
    stats.or_range = or_high - or_low;
    stats.or_midpoint = (or_high + or_low) / 2.0;

    // Get day metrics
    stats.day_high = or_high;
    stats.day_low = or_low;
    stats.day_close = std::get<3>(bars.back());

    for (const auto& bar : bars) {
        stats.day_high = std::max(stats.day_high, std::get<1>(bar));
        stats.day_low = std::min(stats.day_low, std::get<2>(bar));
    }
    stats.day_range = stats.day_high - stats.day_low;

    // Detect breakouts
    stats.broke_high = (stats.day_high > or_high + 0.0001);  // Small epsilon for floating point
    stats.broke_low = (stats.day_low < or_low - 0.0001);

    // Calculate breakout extension
    if (stats.broke_high) {
        stats.breakout_extension = stats.day_high - or_high;
        stats.breakout_time_mins = calculate_breakout_time(bars, period_end_ns, or_high, true);
    } else if (stats.broke_low) {
        stats.breakout_extension = or_low - stats.day_low;
        stats.breakout_time_mins = calculate_breakout_time(bars, period_end_ns, or_low, false);
    } else {
        stats.breakout_extension = 0.0;
        stats.breakout_time_mins = 0.0;
    }

    // Calculate ratios
    if (stats.day_range > 1e-8) {
        stats.or_to_day_ratio = stats.or_range / stats.day_range;

        // Efficiency ratio: Net directional move / Total range
        double net_move = std::abs(stats.day_close - std::get<0>(bars.front()));
        stats.efficiency_ratio = net_move / stats.day_range;
    } else {
        stats.or_to_day_ratio = 0.0;
        stats.efficiency_ratio = 0.0;
    }

    return stats;
}

ORBSummary ORBAnalyzer::summarize(
    const std::string& instrument,
    const std::vector<ORBStats>& daily_stats
) {
    ORBSummary summary;
    summary.instrument = instrument;
    summary.period_minutes = period_minutes_;
    summary.total_days = daily_stats.size();

    if (daily_stats.empty()) {
        return summary;
    }

    // Count breakouts
    summary.high_breakouts = std::count_if(
        daily_stats.begin(), daily_stats.end(),
        [](const ORBStats& s) { return s.broke_high; }
    );

    summary.low_breakouts = std::count_if(
        daily_stats.begin(), daily_stats.end(),
        [](const ORBStats& s) { return s.broke_low; }
    );

    summary.high_breakout_pct = 100.0 * summary.high_breakouts / summary.total_days;
    summary.low_breakout_pct = 100.0 * summary.low_breakouts / summary.total_days;

    // Calculate averages
    summary.avg_or_range = std::accumulate(
        daily_stats.begin(), daily_stats.end(), 0.0,
        [](double sum, const ORBStats& s) { return sum + s.or_range; }
    ) / summary.total_days;

    summary.avg_day_range = std::accumulate(
        daily_stats.begin(), daily_stats.end(), 0.0,
        [](double sum, const ORBStats& s) { return sum + s.day_range; }
    ) / summary.total_days;

    summary.avg_or_to_day_ratio = std::accumulate(
        daily_stats.begin(), daily_stats.end(), 0.0,
        [](double sum, const ORBStats& s) { return sum + s.or_to_day_ratio; }
    ) / summary.total_days;

    // Average breakout extension (only for days with breakouts)
    int breakout_days = summary.high_breakouts + summary.low_breakouts;
    if (breakout_days > 0) {
        summary.avg_breakout_extension = std::accumulate(
            daily_stats.begin(), daily_stats.end(), 0.0,
            [](double sum, const ORBStats& s) {
                return sum + ((s.broke_high || s.broke_low) ? s.breakout_extension : 0.0);
            }
        ) / breakout_days;
    } else {
        summary.avg_breakout_extension = 0.0;
    }

    // Simulated profitability (simple strategy: trade breakout direction)
    summary.total_pnl = 0.0;
    int winning_days = 0;

    for (const auto& day : daily_stats) {
        double day_pnl = 0.0;

        if (day.broke_high) {
            // Simulate long from OR high to day close
            day_pnl = day.day_close - day.or_high;
        } else if (day.broke_low) {
            // Simulate short from OR low to day close
            day_pnl = day.or_low - day.day_close;
        }

        summary.total_pnl += day_pnl;
        if (day_pnl > 0) winning_days++;
    }

    summary.win_rate = breakout_days > 0 ? 100.0 * winning_days / breakout_days : 0.0;

    // Profit factor (gross profit / gross loss)
    double gross_profit = 0.0;
    double gross_loss = 0.0;

    for (const auto& day : daily_stats) {
        double day_pnl = 0.0;

        if (day.broke_high) {
            day_pnl = day.day_close - day.or_high;
        } else if (day.broke_low) {
            day_pnl = day.or_low - day.day_close;
        }

        if (day_pnl > 0) {
            gross_profit += day_pnl;
        } else {
            gross_loss += std::abs(day_pnl);
        }
    }

    summary.profit_factor = gross_loss > 1e-8 ? gross_profit / gross_loss : 0.0;

    return summary;
}

std::pair<double, double> ORBAnalyzer::calculate_opening_range(
    const std::vector<std::tuple<double, double, double, double, int64_t>>& bars,
    int64_t session_start_ns,
    int64_t period_end_ns
) {
    double or_high = -std::numeric_limits<double>::infinity();
    double or_low = std::numeric_limits<double>::infinity();

    for (const auto& bar : bars) {
        int64_t bar_time = std::get<4>(bar);

        if (bar_time >= session_start_ns && bar_time <= period_end_ns) {
            or_high = std::max(or_high, std::get<1>(bar));
            or_low = std::min(or_low, std::get<2>(bar));
        }

        if (bar_time > period_end_ns) {
            break;
        }
    }

    // If no bars found, use first bar
    if (or_high == -std::numeric_limits<double>::infinity()) {
        or_high = std::get<1>(bars.front());
        or_low = std::get<2>(bars.front());
    }

    return {or_high, or_low};
}

double ORBAnalyzer::calculate_breakout_time(
    const std::vector<std::tuple<double, double, double, double, int64_t>>& bars,
    int64_t period_end_ns,
    double threshold_price,
    bool looking_for_high
) {
    for (const auto& bar : bars) {
        int64_t bar_time = std::get<4>(bar);

        if (bar_time <= period_end_ns) {
            continue;
        }

        bool breakout = looking_for_high
            ? (std::get<1>(bar) > threshold_price)
            : (std::get<2>(bar) < threshold_price);

        if (breakout) {
            // Return minutes since period end
            return static_cast<double>(bar_time - period_end_ns) / (60.0 * 1e9);
        }
    }

    return 0.0;  // No breakout found
}

} // namespace quantumliquidity::analytics
