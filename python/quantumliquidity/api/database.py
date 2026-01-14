"""
Database adapter with connection pooling and caching
Optimized for low latency
"""

from sqlalchemy.ext.asyncio import create_async_engine, AsyncSession, async_sessionmaker
from sqlalchemy.orm import declarative_base
from sqlalchemy import text
from typing import AsyncGenerator, Optional
import logging

from .config import settings

logger = logging.getLogger(__name__)

# SQLAlchemy Base
Base = declarative_base()

# Async engine with connection pooling
engine = create_async_engine(
    settings.database_url,
    pool_size=settings.db_pool_size,
    max_overflow=settings.db_max_overflow,
    pool_pre_ping=True,  # Verify connections before use
    pool_recycle=3600,  # Recycle connections after 1 hour
    echo=False,  # Disable SQL logging for performance
)

# Session factory
AsyncSessionLocal = async_sessionmaker(
    engine,
    class_=AsyncSession,
    expire_on_commit=False,
    autocommit=False,
    autoflush=False,
)


async def get_db() -> AsyncGenerator[AsyncSession, None]:
    """
    Dependency for FastAPI routes
    Provides async database session
    """
    async with AsyncSessionLocal() as session:
        try:
            yield session
        finally:
            await session.close()


async def init_db():
    """Initialize database connection"""
    try:
        async with engine.begin() as conn:
            # Test connection
            await conn.execute(text("SELECT 1"))
        logger.info("Database connected: %s", settings.db_host)
    except Exception as e:
        logger.error("Database connection failed: %s", e)
        raise


async def close_db():
    """Close database connections"""
    await engine.dispose()
    logger.info("Database connections closed")


# ==================== Query Helpers ====================

class DatabaseService:
    """
    Database service with common queries
    All queries optimized with indexes
    """

    def __init__(self, session: AsyncSession):
        self.session = session

    # ==================== Orders ====================

    async def get_orders(
        self,
        limit: int = 100,
        offset: int = 0,
        status: Optional[str] = None,
        instrument: Optional[str] = None,
    ):
        """Get orders with pagination and filters"""
        query = """
            SELECT order_id, client_order_id, strategy_id, instrument,
                   side, order_type, quantity, limit_price, stop_price,
                   time_in_force, status, filled_quantity, remaining_quantity,
                   average_fill_price, reject_reason, created_at, submitted_at,
                   filled_at, provider, user_comment
            FROM orders
            WHERE 1=1
        """
        params = {"limit": limit, "offset": offset}

        if status:
            query += " AND status = :status"
            params["status"] = status

        if instrument:
            query += " AND instrument = :instrument"
            params["instrument"] = instrument

        query += " ORDER BY created_at DESC LIMIT :limit OFFSET :offset"

        result = await self.session.execute(text(query), params)
        return result.fetchall()

    async def get_order_by_id(self, order_id: int):
        """Get single order by ID"""
        query = text("""
            SELECT * FROM orders WHERE order_id = :order_id
        """)
        result = await self.session.execute(query, {"order_id": order_id})
        return result.fetchone()

    # ==================== Positions ====================

    async def get_latest_positions(self):
        """Get latest positions for all instruments"""
        query = text("""
            SELECT DISTINCT ON (instrument)
                timestamp, instrument, quantity, entry_price,
                unrealized_pnl, realized_pnl, num_fills_today,
                total_commission, last_update_ns
            FROM positions
            WHERE quantity != 0
            ORDER BY instrument, timestamp DESC
        """)
        result = await self.session.execute(query)
        return result.fetchall()

    async def get_position(self, instrument: str):
        """Get latest position for instrument"""
        query = text("""
            SELECT timestamp, instrument, quantity, entry_price,
                   unrealized_pnl, realized_pnl, num_fills_today,
                   total_commission, last_update_ns
            FROM positions
            WHERE instrument = :instrument
            ORDER BY timestamp DESC
            LIMIT 1
        """)
        result = await self.session.execute(query, {"instrument": instrument})
        return result.fetchone()

    # ==================== Trades ====================

    async def get_trades(
        self,
        limit: int = 100,
        offset: int = 0,
        instrument: Optional[str] = None,
        from_timestamp: Optional[int] = None,
    ):
        """Get trades with pagination"""
        query = """
            SELECT trade_id, fill_id, order_id, client_order_id,
                   execution_id, timestamp, instrument, side,
                   quantity, price, commission, pnl, provider
            FROM trades
            WHERE 1=1
        """
        params = {"limit": limit, "offset": offset}

        if instrument:
            query += " AND instrument = :instrument"
            params["instrument"] = instrument

        if from_timestamp:
            query += " AND EXTRACT(EPOCH FROM timestamp) >= :from_ts"
            params["from_ts"] = from_timestamp

        query += " ORDER BY timestamp DESC LIMIT :limit OFFSET :offset"

        result = await self.session.execute(text(query), params)
        return result.fetchall()

    async def get_trade_count(self, instrument: Optional[str] = None):
        """Get total trade count"""
        query = "SELECT COUNT(*) FROM trades"
        params = {}

        if instrument:
            query += " WHERE instrument = :instrument"
            params["instrument"] = instrument

        result = await self.session.execute(text(query), params)
        return result.scalar()

    # ==================== Risk Metrics ====================

    async def get_latest_risk_metrics(self):
        """Get latest risk snapshot"""
        query = text("""
            SELECT timestamp, total_exposure, account_utilization,
                   daily_pnl, realized_pnl, unrealized_pnl,
                   max_dd_today, daily_high_pnl,
                   orders_submitted_today, orders_filled_today,
                   orders_rejected_today, orders_cancelled_today,
                   halt_active, halt_reason
            FROM risk_metrics
            ORDER BY timestamp DESC
            LIMIT 1
        """)
        result = await self.session.execute(query)
        return result.fetchone()

    # ==================== Analytics ====================

    async def get_daily_stats(
        self,
        instrument: str,
        days: int = 30
    ):
        """Get daily statistics"""
        query = text("""
            SELECT date, instrument, day_type, open_price, high_price,
                   low_price, close_price, range_points, atr, trend_strength
            FROM daily_stats
            WHERE instrument = :instrument
            ORDER BY date DESC
            LIMIT :days
        """)
        result = await self.session.execute(
            query,
            {"instrument": instrument, "days": days}
        )
        return result.fetchall()

    async def get_orb_stats(
        self,
        instrument: str,
        days: int = 30,
        period: int = 30
    ):
        """Get ORB statistics"""
        query = text("""
            SELECT date, instrument, or_period_minutes, or_high, or_low,
                   or_size, breakout_occurred, breakout_direction,
                   breakout_time, max_r_multiple_up, max_r_multiple_down
            FROM orb_stats
            WHERE instrument = :instrument
              AND or_period_minutes = :period
            ORDER BY date DESC
            LIMIT :days
        """)
        result = await self.session.execute(
            query,
            {"instrument": instrument, "period": period, "days": days}
        )
        return result.fetchall()

    async def get_volume_profile(self, instrument: str, days: int = 30):
        """Get volume profile"""
        query = text("""
            SELECT date, instrument, value_area_high, value_area_low,
                   point_of_control, profile_type
            FROM volume_profile
            WHERE instrument = :instrument
            ORDER BY date DESC
            LIMIT :days
        """)
        result = await self.session.execute(
            query,
            {"instrument": instrument, "days": days}
        )
        return result.fetchall()

    # ==================== Market Data ====================

    async def get_latest_ticks(
        self,
        instrument: str,
        limit: int = 100
    ):
        """Get recent ticks"""
        query = text("""
            SELECT timestamp, instrument, bid, ask, bid_size, ask_size,
                   last_trade_price, last_trade_size, provider
            FROM ticks
            WHERE instrument = :instrument
            ORDER BY timestamp DESC
            LIMIT :limit
        """)
        result = await self.session.execute(
            query,
            {"instrument": instrument, "limit": limit}
        )
        return result.fetchall()

    async def get_bars(
        self,
        instrument: str,
        timeframe: str = "1m",
        limit: int = 500
    ):
        """Get OHLCV bars"""
        table_name = f"bars_{timeframe}"
        query = text(f"""
            SELECT timestamp, instrument, open, high, low, close,
                   volume, tick_count, provider
            FROM {table_name}
            WHERE instrument = :instrument
            ORDER BY timestamp DESC
            LIMIT :limit
        """)
        result = await self.session.execute(
            query,
            {"instrument": instrument, "limit": limit}
        )
        return result.fetchall()

    # ==================== Sentiment ====================

    async def get_news_events(
        self,
        instrument: Optional[str] = None,
        limit: int = 100
    ):
        """Get recent news events"""
        query = """
            SELECT id, timestamp, title, content, source, instruments,
                   sentiment, event_type, confidence
            FROM news_events
            WHERE 1=1
        """
        params = {"limit": limit}

        if instrument:
            query += " AND :instrument = ANY(instruments)"
            params["instrument"] = instrument

        query += " ORDER BY timestamp DESC LIMIT :limit"

        result = await self.session.execute(text(query), params)
        return result.fetchall()

    async def get_sentiment_series(
        self,
        instrument: str,
        limit: int = 100
    ):
        """Get sentiment time series"""
        query = text("""
            SELECT timestamp, instrument, sentiment_score,
                   event_count, avg_confidence
            FROM sentiment_series
            WHERE instrument = :instrument
            ORDER BY timestamp DESC
            LIMIT :limit
        """)
        result = await self.session.execute(
            query,
            {"instrument": instrument, "limit": limit}
        )
        return result.fetchall()
