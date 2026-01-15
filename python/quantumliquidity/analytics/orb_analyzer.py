"""
Opening Range Breakout (ORB) Analyzer
Statistical analysis of opening range breakouts
"""

from dataclasses import dataclass
from typing import List, Tuple
from datetime import datetime, timedelta


@dataclass
class ORBStats:
    """Statistics for a single day's ORB analysis"""

    instrument: str
    period_minutes: int
    date: str

    # Opening range metrics
    or_high: float
    or_low: float
    or_range: float
    or_midpoint: float

    # Day metrics
    day_high: float
    day_low: float
    day_close: float
    day_range: float

    # Breakout analysis
    broke_high: bool
    broke_low: bool
    breakout_extension: float  # How far beyond OR
    breakout_time_mins: float  # When breakout occurred

    # Statistical metrics
    or_to_day_ratio: float  # OR range / Day range
    efficiency_ratio: float  # Net move / Total range


@dataclass
class ORBSummary:
    """Summary statistics over multiple days"""

    instrument: str
    period_minutes: int
    total_days: int

    # Breakout statistics
    high_breakouts: int
    low_breakouts: int
    high_breakout_pct: float
    low_breakout_pct: float

    # Average metrics
    avg_or_range: float
    avg_day_range: float
    avg_or_to_day_ratio: float
    avg_breakout_extension: float

    # Profitability (simulated)
    total_pnl: float
    win_rate: float
    profit_factor: float


class ORBAnalyzer:
    """
    Analyzes Opening Range Breakout patterns
    """

    def __init__(self, period_minutes: int = 30):
        self.period_minutes = period_minutes

    def analyze_day(
        self,
        instrument: str,
        bars: List[Tuple[float, float, float, float, str]],
        session_start: datetime,
    ) -> ORBStats:
        """
        Analyze single day for ORB statistics
        bars: List of (open, high, low, close, timestamp_str) tuples
        """

        if not bars:
            return self._empty_stats(instrument, session_start)

        # Calculate period end time
        period_end = session_start + timedelta(minutes=self.period_minutes)

        # Get opening range
        or_high, or_low = self._calculate_opening_range(bars, session_start, period_end)
        or_range = or_high - or_low
        or_midpoint = (or_high + or_low) / 2.0

        # Get day metrics
        day_high = max(bar[1] for bar in bars)
        day_low = min(bar[2] for bar in bars)
        day_close = bars[-1][3]
        day_range = day_high - day_low

        # Detect breakouts
        broke_high = day_high > or_high + 0.0001
        broke_low = day_low < or_low - 0.0001

        # Calculate breakout extension and time
        if broke_high:
            breakout_extension = day_high - or_high
            breakout_time_mins = self._calculate_breakout_time(
                bars, period_end, or_high, looking_for_high=True
            )
        elif broke_low:
            breakout_extension = or_low - day_low
            breakout_time_mins = self._calculate_breakout_time(
                bars, period_end, or_low, looking_for_high=False
            )
        else:
            breakout_extension = 0.0
            breakout_time_mins = 0.0

        # Calculate ratios
        or_to_day_ratio = or_range / day_range if day_range > 1e-8 else 0.0

        # Efficiency ratio: Net directional move / Total range
        net_move = abs(day_close - bars[0][0])
        efficiency_ratio = net_move / day_range if day_range > 1e-8 else 0.0

        return ORBStats(
            instrument=instrument,
            period_minutes=self.period_minutes,
            date=session_start.strftime("%Y-%m-%d"),
            or_high=or_high,
            or_low=or_low,
            or_range=or_range,
            or_midpoint=or_midpoint,
            day_high=day_high,
            day_low=day_low,
            day_close=day_close,
            day_range=day_range,
            broke_high=broke_high,
            broke_low=broke_low,
            breakout_extension=breakout_extension,
            breakout_time_mins=breakout_time_mins,
            or_to_day_ratio=or_to_day_ratio,
            efficiency_ratio=efficiency_ratio,
        )

    def summarize(self, instrument: str, daily_stats: List[ORBStats]) -> ORBSummary:
        """
        Generate summary statistics over multiple days
        """

        if not daily_stats:
            return ORBSummary(
                instrument=instrument,
                period_minutes=self.period_minutes,
                total_days=0,
                high_breakouts=0,
                low_breakouts=0,
                high_breakout_pct=0.0,
                low_breakout_pct=0.0,
                avg_or_range=0.0,
                avg_day_range=0.0,
                avg_or_to_day_ratio=0.0,
                avg_breakout_extension=0.0,
                total_pnl=0.0,
                win_rate=0.0,
                profit_factor=0.0,
            )

        total_days = len(daily_stats)

        # Count breakouts
        high_breakouts = sum(1 for s in daily_stats if s.broke_high)
        low_breakouts = sum(1 for s in daily_stats if s.broke_low)

        high_breakout_pct = 100.0 * high_breakouts / total_days
        low_breakout_pct = 100.0 * low_breakouts / total_days

        # Calculate averages
        avg_or_range = sum(s.or_range for s in daily_stats) / total_days
        avg_day_range = sum(s.day_range for s in daily_stats) / total_days
        avg_or_to_day_ratio = sum(s.or_to_day_ratio for s in daily_stats) / total_days

        # Average breakout extension (only for days with breakouts)
        breakout_days = high_breakouts + low_breakouts
        if breakout_days > 0:
            avg_breakout_extension = sum(
                s.breakout_extension for s in daily_stats if (s.broke_high or s.broke_low)
            ) / breakout_days
        else:
            avg_breakout_extension = 0.0

        # Simulated profitability (simple strategy: trade breakout direction)
        total_pnl = 0.0
        winning_days = 0
        gross_profit = 0.0
        gross_loss = 0.0

        for day in daily_stats:
            day_pnl = 0.0

            if day.broke_high:
                # Simulate long from OR high to day close
                day_pnl = day.day_close - day.or_high
            elif day.broke_low:
                # Simulate short from OR low to day close
                day_pnl = day.or_low - day.day_close

            total_pnl += day_pnl

            if day_pnl > 0:
                winning_days += 1
                gross_profit += day_pnl
            else:
                gross_loss += abs(day_pnl)

        win_rate = 100.0 * winning_days / breakout_days if breakout_days > 0 else 0.0
        profit_factor = gross_profit / gross_loss if gross_loss > 1e-8 else 0.0

        return ORBSummary(
            instrument=instrument,
            period_minutes=self.period_minutes,
            total_days=total_days,
            high_breakouts=high_breakouts,
            low_breakouts=low_breakouts,
            high_breakout_pct=high_breakout_pct,
            low_breakout_pct=low_breakout_pct,
            avg_or_range=avg_or_range,
            avg_day_range=avg_day_range,
            avg_or_to_day_ratio=avg_or_to_day_ratio,
            avg_breakout_extension=avg_breakout_extension,
            total_pnl=total_pnl,
            win_rate=win_rate,
            profit_factor=profit_factor,
        )

    def _empty_stats(self, instrument: str, session_start: datetime) -> ORBStats:
        """Return empty stats when no data"""
        return ORBStats(
            instrument=instrument,
            period_minutes=self.period_minutes,
            date=session_start.strftime("%Y-%m-%d"),
            or_high=0.0,
            or_low=0.0,
            or_range=0.0,
            or_midpoint=0.0,
            day_high=0.0,
            day_low=0.0,
            day_close=0.0,
            day_range=0.0,
            broke_high=False,
            broke_low=False,
            breakout_extension=0.0,
            breakout_time_mins=0.0,
            or_to_day_ratio=0.0,
            efficiency_ratio=0.0,
        )

    def _calculate_opening_range(
        self,
        bars: List[Tuple[float, float, float, float, str]],
        session_start: datetime,
        period_end: datetime,
    ) -> Tuple[float, float]:
        """Calculate opening range high and low"""

        or_high = float("-inf")
        or_low = float("inf")

        for bar in bars:
            bar_time = datetime.fromisoformat(bar[4].replace("Z", "+00:00"))

            if session_start <= bar_time <= period_end:
                or_high = max(or_high, bar[1])
                or_low = min(or_low, bar[2])

            if bar_time > period_end:
                break

        # If no bars found, use first bar
        if or_high == float("-inf"):
            or_high = bars[0][1]
            or_low = bars[0][2]

        return or_high, or_low

    def _calculate_breakout_time(
        self,
        bars: List[Tuple[float, float, float, float, str]],
        period_end: datetime,
        threshold_price: float,
        looking_for_high: bool,
    ) -> float:
        """Calculate when breakout occurred (minutes after period end)"""

        for bar in bars:
            bar_time = datetime.fromisoformat(bar[4].replace("Z", "+00:00"))

            if bar_time <= period_end:
                continue

            breakout = bar[1] > threshold_price if looking_for_high else bar[2] < threshold_price

            if breakout:
                # Return minutes since period end
                delta = bar_time - period_end
                return delta.total_seconds() / 60.0

        return 0.0  # No breakout found
