"""Analytics API routes"""

from fastapi import APIRouter, Depends, Query
from sqlalchemy.ext.asyncio import AsyncSession
from typing import List
from datetime import datetime, timedelta

from ..database import get_db, DatabaseService
from ..cache import cached
from ..schemas import DailyStats, ORBStats, VolumeProfile
from ...analytics import DayClassifier, ORBAnalyzer

router = APIRouter()


@router.get("/daily/{instrument}", response_model=List[DailyStats])
@cached("analytics:daily", ttl=5)
async def get_daily_stats(
    instrument: str,
    days: int = Query(30, le=365),
    db: AsyncSession = Depends(get_db)
):
    """Get daily statistics"""
    db_service = DatabaseService(db)
    rows = await db_service.get_daily_stats(instrument, days)
    return [DailyStats(**dict(row._mapping)) for row in rows]


@router.get("/orb/{instrument}", response_model=List[ORBStats])
@cached("analytics:orb", ttl=5)
async def get_orb_stats(
    instrument: str,
    period: int = Query(30, description="ORB period in minutes"),
    days: int = Query(30, le=365),
    db: AsyncSession = Depends(get_db)
):
    """Get ORB statistics"""
    db_service = DatabaseService(db)
    rows = await db_service.get_orb_stats(instrument, days, period)
    return [ORBStats(**dict(row._mapping)) for row in rows]


@router.get("/volume-profile/{instrument}", response_model=List[VolumeProfile])
@cached("analytics:volume", ttl=5)
async def get_volume_profile(
    instrument: str,
    days: int = Query(30, le=365),
    db: AsyncSession = Depends(get_db)
):
    """Get volume profile"""
    db_service = DatabaseService(db)
    rows = await db_service.get_volume_profile(instrument, days)
    return [VolumeProfile(**dict(row._mapping)) for row in rows]


@router.get("/day-classification/{instrument}")
@cached("analytics:day_class", ttl=1)
async def classify_today(
    instrument: str,
    db: AsyncSession = Depends(get_db)
):
    """
    Classify today's trading day
    Returns: Day type (TREND_UP, TREND_DOWN, RANGE, V_DAY, P_DAY) with confidence
    """
    db_service = DatabaseService(db)

    # Get today's bars (1-minute)
    today = datetime.now().date()
    bars = await db_service.get_bars_for_date(instrument, today, timeframe="1m")

    if not bars:
        return {
            "instrument": instrument,
            "date": today.isoformat(),
            "type": "UNDEFINED",
            "confidence": 0.0,
            "message": "No data available for today"
        }

    # Convert bars to format: (open, high, low, close, timestamp)
    bar_data = [
        (row.open, row.high, row.low, row.close, row.timestamp.isoformat())
        for row in bars
    ]

    classifier = DayClassifier()
    stats = classifier.classify_from_bars(bar_data)

    return {
        "instrument": instrument,
        "date": today.isoformat(),
        "type": stats.type.value,
        "confidence": stats.confidence,
        "open": stats.open,
        "high": stats.high,
        "low": stats.low,
        "close": stats.close,
        "range": stats.range,
        "body_pct": stats.body_pct,
        "wick_top_pct": stats.wick_top_pct,
        "wick_bottom_pct": stats.wick_bottom_pct,
        "volatility": stats.volatility,
    }


@router.get("/day-classification/{instrument}/history")
@cached("analytics:day_class_history", ttl=5)
async def classify_history(
    instrument: str,
    days: int = Query(30, le=90),
    db: AsyncSession = Depends(get_db)
):
    """
    Classify multiple days
    Returns: Historical day classifications with statistics
    """
    db_service = DatabaseService(db)
    classifier = DayClassifier()

    results = []
    today = datetime.now().date()

    for i in range(days):
        target_date = today - timedelta(days=i)
        bars = await db_service.get_bars_for_date(instrument, target_date, timeframe="1m")

        if not bars:
            continue

        bar_data = [
            (row.open, row.high, row.low, row.close, row.timestamp.isoformat())
            for row in bars
        ]

        stats = classifier.classify_from_bars(bar_data)

        results.append({
            "date": target_date.isoformat(),
            "type": stats.type.value,
            "confidence": stats.confidence,
            "range": stats.range,
            "body_pct": stats.body_pct,
        })

    # Calculate statistics
    if results:
        type_counts = {}
        for r in results:
            day_type = r["type"]
            type_counts[day_type] = type_counts.get(day_type, 0) + 1

        return {
            "instrument": instrument,
            "days_analyzed": len(results),
            "classifications": results,
            "summary": {
                "type_distribution": type_counts,
                "avg_confidence": sum(r["confidence"] for r in results) / len(results),
            }
        }

    return {
        "instrument": instrument,
        "days_analyzed": 0,
        "classifications": [],
        "summary": {}
    }


@router.get("/orb-analysis/{instrument}")
@cached("analytics:orb_analysis", ttl=1)
async def analyze_orb_today(
    instrument: str,
    period: int = Query(30, description="ORB period in minutes", ge=15, le=120),
    db: AsyncSession = Depends(get_db)
):
    """
    Analyze today's Opening Range Breakout
    Returns: ORB stats with breakout detection
    """
    db_service = DatabaseService(db)

    # Get today's bars (1-minute)
    today = datetime.now().date()
    bars = await db_service.get_bars_for_date(instrument, today, timeframe="1m")

    if not bars:
        return {
            "instrument": instrument,
            "date": today.isoformat(),
            "message": "No data available for today"
        }

    # Convert bars
    bar_data = [
        (row.open, row.high, row.low, row.close, row.timestamp.isoformat())
        for row in bars
    ]

    # Assume session starts at first bar
    session_start = bars[0].timestamp

    analyzer = ORBAnalyzer(period_minutes=period)
    stats = analyzer.analyze_day(instrument, bar_data, session_start)

    return {
        "instrument": stats.instrument,
        "date": stats.date,
        "period_minutes": stats.period_minutes,
        "or_high": stats.or_high,
        "or_low": stats.or_low,
        "or_range": stats.or_range,
        "or_midpoint": stats.or_midpoint,
        "day_high": stats.day_high,
        "day_low": stats.day_low,
        "day_close": stats.day_close,
        "day_range": stats.day_range,
        "broke_high": stats.broke_high,
        "broke_low": stats.broke_low,
        "breakout_extension": stats.breakout_extension,
        "breakout_time_mins": stats.breakout_time_mins,
        "or_to_day_ratio": stats.or_to_day_ratio,
        "efficiency_ratio": stats.efficiency_ratio,
    }


@router.get("/orb-analysis/{instrument}/summary")
@cached("analytics:orb_summary", ttl=5)
async def orb_summary(
    instrument: str,
    period: int = Query(30, description="ORB period in minutes", ge=15, le=120),
    days: int = Query(30, le=90),
    db: AsyncSession = Depends(get_db)
):
    """
    Generate ORB summary statistics over multiple days
    Returns: Breakout frequency, win rate, profit factor
    """
    db_service = DatabaseService(db)
    analyzer = ORBAnalyzer(period_minutes=period)

    daily_stats = []
    today = datetime.now().date()

    for i in range(days):
        target_date = today - timedelta(days=i)
        bars = await db_service.get_bars_for_date(instrument, target_date, timeframe="1m")

        if not bars:
            continue

        bar_data = [
            (row.open, row.high, row.low, row.close, row.timestamp.isoformat())
            for row in bars
        ]

        session_start = bars[0].timestamp
        stats = analyzer.analyze_day(instrument, bar_data, session_start)
        daily_stats.append(stats)

    if not daily_stats:
        return {
            "instrument": instrument,
            "period_minutes": period,
            "message": "No data available"
        }

    summary = analyzer.summarize(instrument, daily_stats)

    return {
        "instrument": summary.instrument,
        "period_minutes": summary.period_minutes,
        "total_days": summary.total_days,
        "high_breakouts": summary.high_breakouts,
        "low_breakouts": summary.low_breakouts,
        "high_breakout_pct": summary.high_breakout_pct,
        "low_breakout_pct": summary.low_breakout_pct,
        "avg_or_range": summary.avg_or_range,
        "avg_day_range": summary.avg_day_range,
        "avg_or_to_day_ratio": summary.avg_or_to_day_ratio,
        "avg_breakout_extension": summary.avg_breakout_extension,
        "simulated_pnl": summary.total_pnl,
        "win_rate": summary.win_rate,
        "profit_factor": summary.profit_factor,
    }
