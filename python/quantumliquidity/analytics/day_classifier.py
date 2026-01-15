"""
Day Classification Engine
Classifies trading days into: Trend, Range, V-Day, P-Day
"""

from enum import Enum
from dataclasses import dataclass
from typing import List, Tuple
import math


class DayType(str, Enum):
    TREND_UP = "TREND_UP"
    TREND_DOWN = "TREND_DOWN"
    RANGE = "RANGE"
    V_DAY = "V_DAY"
    P_DAY = "P_DAY"
    UNDEFINED = "UNDEFINED"


@dataclass
class DayStats:
    type: DayType
    open: float
    high: float
    low: float
    close: float
    range: float
    body_pct: float  # Body as % of range
    wick_top_pct: float  # Upper wick as % of range
    wick_bottom_pct: float  # Lower wick as % of range
    volatility: float  # Intraday volatility
    confidence: float  # Classification confidence (0-1)
    timestamp: str


class DayClassifier:
    """
    Classifies trading days based on price action patterns
    """

    # Classification thresholds
    TREND_THRESHOLD = 0.7  # Body must be >70% of range
    RANGE_THRESHOLD = 0.4  # Body must be <40% of range
    V_DAY_THRESHOLD = 0.3  # Both wicks >30% of range
    P_DAY_THRESHOLD = 0.6  # Progressive move >60%

    def classify(
        self,
        open_price: float,
        high: float,
        low: float,
        close: float,
        timestamp: str,
    ) -> DayStats:
        """
        Classify a single day based on OHLC data
        """

        # Calculate range
        range_val = high - low
        if range_val < 1e-8:
            return DayStats(
                type=DayType.UNDEFINED,
                open=open_price,
                high=high,
                low=low,
                close=close,
                range=0.0,
                body_pct=0.0,
                wick_top_pct=0.0,
                wick_bottom_pct=0.0,
                volatility=0.0,
                confidence=0.0,
                timestamp=timestamp,
            )

        # Calculate body and wicks as percentage of range
        body = abs(close - open_price)
        upper_wick = high - max(open_price, close)
        lower_wick = min(open_price, close) - low

        body_pct = body / range_val
        wick_top_pct = upper_wick / range_val
        wick_bottom_pct = lower_wick / range_val

        # Calculate volatility (simplified)
        volatility = range_val / open_price

        # Determine day type
        day_type = self._determine_type(body_pct, wick_top_pct, wick_bottom_pct, open_price, close)
        confidence = self._calculate_confidence(body_pct, wick_top_pct, wick_bottom_pct, day_type)

        return DayStats(
            type=day_type,
            open=open_price,
            high=high,
            low=low,
            close=close,
            range=range_val,
            body_pct=body_pct,
            wick_top_pct=wick_top_pct,
            wick_bottom_pct=wick_bottom_pct,
            volatility=volatility,
            confidence=confidence,
            timestamp=timestamp,
        )

    def classify_from_bars(
        self,
        bars: List[Tuple[float, float, float, float, str]],
    ) -> DayStats:
        """
        Classify a day from intraday bar data
        bars: List of (open, high, low, close, timestamp) tuples
        """

        if not bars:
            return DayStats(
                type=DayType.UNDEFINED,
                open=0.0,
                high=0.0,
                low=0.0,
                close=0.0,
                range=0.0,
                body_pct=0.0,
                wick_top_pct=0.0,
                wick_bottom_pct=0.0,
                volatility=0.0,
                confidence=0.0,
                timestamp="",
            )

        # Get session OHLC
        open_price = bars[0][0]
        close = bars[-1][3]
        timestamp = bars[-1][4]

        high = max(bar[1] for bar in bars)
        low = min(bar[2] for bar in bars)

        return self.classify(open_price, high, low, close, timestamp)

    def _determine_type(
        self,
        body_pct: float,
        wick_top_pct: float,
        wick_bottom_pct: float,
        open_price: float,
        close: float,
    ) -> DayType:
        """
        Determine day type based on percentages
        """

        # V-Day: Large wicks on both sides (reversal pattern)
        if wick_top_pct > self.V_DAY_THRESHOLD and wick_bottom_pct > self.V_DAY_THRESHOLD:
            return DayType.V_DAY

        # Trend: Large body, small wicks
        if body_pct > self.TREND_THRESHOLD:
            return DayType.TREND_UP if close > open_price else DayType.TREND_DOWN

        # Range: Small body relative to range
        if body_pct < self.RANGE_THRESHOLD:
            return DayType.RANGE

        # P-Day: Progressive move with one dominant wick
        if body_pct > self.P_DAY_THRESHOLD:
            if close > open_price and wick_bottom_pct < 0.15:
                return DayType.P_DAY
            if close < open_price and wick_top_pct < 0.15:
                return DayType.P_DAY

        return DayType.UNDEFINED

    def _calculate_confidence(
        self,
        body_pct: float,
        wick_top_pct: float,
        wick_bottom_pct: float,
        day_type: DayType,
    ) -> float:
        """
        Calculate classification confidence (0-1)
        """

        if day_type in (DayType.TREND_UP, DayType.TREND_DOWN):
            # Confidence based on body percentage
            return min(1.0, body_pct / self.TREND_THRESHOLD)

        elif day_type == DayType.RANGE:
            # Confidence inversely related to body size
            return min(1.0, 1.0 - (body_pct / self.RANGE_THRESHOLD))

        elif day_type == DayType.V_DAY:
            # Confidence based on wick symmetry
            return min(1.0, (wick_top_pct + wick_bottom_pct) / (2 * self.V_DAY_THRESHOLD))

        elif day_type == DayType.P_DAY:
            # Confidence based on body dominance
            return min(1.0, body_pct / self.P_DAY_THRESHOLD)

        return 0.0
