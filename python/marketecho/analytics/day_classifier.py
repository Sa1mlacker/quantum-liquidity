"""Day type classification

Classifies trading days into categories:
- Trend day: Sustained directional movement
- Range day: Bounded by support/resistance
- V-day: Sharp reversal intraday
- P-day: Multiple pushes in one direction
- Normal variation day
"""

from datetime import date
from decimal import Decimal
from typing import List
from enum import Enum
import numpy as np
from pydantic import BaseModel

from quantumliquidity.common import Bar


class DayType(str, Enum):
    TREND = "TREND"
    RANGE = "RANGE"
    V_DAY = "V_DAY"
    P_DAY = "P_DAY"
    NORMAL = "NORMAL"


class DayProfile(BaseModel):
    """Day classification result"""
    date: date
    instrument: str
    day_type: DayType
    open_price: Decimal
    high_price: Decimal
    low_price: Decimal
    close_price: Decimal
    range_points: Decimal
    atr: Decimal
    trend_strength: float  # 0-1
    range_efficiency: float  # 0-1
    reversal_score: float  # 0-1


class DayClassifier:
    """Classify day types based on intraday price action"""

    def __init__(self, lookback_days: int = 20):
        self.lookback_days = lookback_days

    def classify_day(self, bars: List[Bar], atr: Decimal) -> DayProfile:
        """
        Classify a single day based on intraday bars

        Args:
            bars: Intraday bars (e.g., 5min or 15min) for one day
            atr: Average True Range for context

        Returns:
            DayProfile with classification
        """
        if not bars:
            raise ValueError("Empty bar list")

        # Extract prices
        opens = np.array([float(bar.open) for bar in bars])
        highs = np.array([float(bar.high) for bar in bars])
        lows = np.array([float(bar.low) for bar in bars])
        closes = np.array([float(bar.close) for bar in bars])

        # Day metrics
        day_open = Decimal(str(opens[0]))
        day_high = Decimal(str(highs.max()))
        day_low = Decimal(str(lows.min()))
        day_close = Decimal(str(closes[-1]))
        day_range = day_high - day_low

        # Calculate metrics
        trend_strength = self._calculate_trend_strength(closes)
        range_efficiency = self._calculate_range_efficiency(bars)
        reversal_score = self._calculate_reversal_score(highs, lows, closes)

        # Classify
        day_type = self._determine_day_type(
            trend_strength, range_efficiency, reversal_score
        )

        return DayProfile(
            date=bars[0].timestamp.date(),
            instrument=bars[0].instrument,
            day_type=day_type,
            open_price=day_open,
            high_price=day_high,
            low_price=day_low,
            close_price=day_close,
            range_points=day_range,
            atr=atr,
            trend_strength=trend_strength,
            range_efficiency=range_efficiency,
            reversal_score=reversal_score
        )

    def _calculate_trend_strength(self, closes: np.ndarray) -> float:
        """
        Measure trend strength using directional movement

        Returns: 0 (no trend) to 1 (strong trend)
        """
        if len(closes) < 2:
            return 0.0

        # Linear regression slope normalized by price range
        x = np.arange(len(closes))
        slope, _ = np.polyfit(x, closes, 1)

        price_range = closes.max() - closes.min()
        if price_range == 0:
            return 0.0

        # Normalize slope by range and number of bars
        normalized_slope = abs(slope * len(closes)) / price_range

        return min(normalized_slope, 1.0)

    def _calculate_range_efficiency(self, bars: List[Bar]) -> float:
        """
        Measure how efficiently price moved (low chop)

        Returns: 0 (choppy) to 1 (efficient trend)
        """
        if len(bars) < 2:
            return 0.0

        # Total price movement vs. net movement
        total_movement = sum(
            abs(float(bars[i].close) - float(bars[i-1].close))
            for i in range(1, len(bars))
        )

        net_movement = abs(float(bars[-1].close) - float(bars[0].open))

        if total_movement == 0:
            return 0.0

        return net_movement / total_movement

    def _calculate_reversal_score(
        self, highs: np.ndarray, lows: np.ndarray, closes: np.ndarray
    ) -> float:
        """
        Detect V-shaped reversals

        Returns: 0 (no reversal) to 1 (sharp V)
        """
        if len(closes) < 3:
            return 0.0

        # Find extreme point (high or low hit during day)
        high_idx = np.argmax(highs)
        low_idx = np.argmin(lows)

        # V-day typically has extreme in middle third of day
        middle_third = len(closes) // 3
        if not (middle_third <= high_idx <= 2 * middle_third or
                middle_third <= low_idx <= 2 * middle_third):
            return 0.0

        # Measure reversal strength
        price_range = highs.max() - lows.min()
        if price_range == 0:
            return 0.0

        # Distance from close to day's extreme
        if high_idx < low_idx:  # High first, then low (inverted V)
            reversal = (highs[high_idx] - closes[-1]) / price_range
        else:  # Low first, then high (V)
            reversal = (closes[-1] - lows[low_idx]) / price_range

        return max(0.0, min(reversal, 1.0))

    def _determine_day_type(
        self, trend_strength: float, range_efficiency: float, reversal_score: float
    ) -> DayType:
        """
        Classify day type based on metrics

        Thresholds tuned for FX/indices (may need adjustment per instrument)
        """
        # V-day: Strong reversal
        if reversal_score > 0.6:
            return DayType.V_DAY

        # Trend day: Strong trend + efficient movement
        if trend_strength > 0.6 and range_efficiency > 0.5:
            return DayType.TREND

        # Range day: Weak trend + choppy
        if trend_strength < 0.3 and range_efficiency < 0.4:
            return DayType.RANGE

        # P-day: Multiple pushes (trend strength medium, but choppy)
        if 0.4 < trend_strength < 0.7 and range_efficiency < 0.5:
            return DayType.P_DAY

        # Default
        return DayType.NORMAL
