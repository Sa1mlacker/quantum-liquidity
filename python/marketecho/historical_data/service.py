"""Historical data service

Handles downloading, normalizing, and storing historical market data.
"""

import asyncpg
from datetime import datetime, date
from decimal import Decimal
from typing import List, Optional
from loguru import logger

from quantumliquidity.common import TimeFrame, Bar, Tick, settings


class HistoricalDataService:
    """Service for managing historical market data"""

    def __init__(self, db_pool: asyncpg.Pool):
        self.db_pool = db_pool

    async def download_bars(
        self,
        instrument: str,
        start_date: date,
        end_date: date,
        timeframe: TimeFrame,
        provider: str = "unknown"
    ) -> int:
        """
        Download historical bars from broker/vendor API

        TODO: Implement provider-specific downloaders:
        - Darwinex API
        - Interactive Brokers TWS
        - MT5 bridge
        - CSV import

        Returns: Number of bars downloaded
        """
        logger.info(
            f"Downloading {timeframe} bars for {instrument} "
            f"from {start_date} to {end_date} via {provider}"
        )

        # TODO: Call provider API to fetch data
        # For now, return 0
        return 0

    async def store_bars(self, bars: List[Bar]) -> None:
        """Store bars in database"""
        if not bars:
            return

        # Determine table name based on timeframe
        table_name = f"bars_{bars[0].timeframe.value}"

        async with self.db_pool.acquire() as conn:
            await conn.executemany(
                f"""
                INSERT INTO {table_name}
                (timestamp, instrument, open, high, low, close, volume, tick_count)
                VALUES ($1, $2, $3, $4, $5, $6, $7, $8)
                ON CONFLICT (timestamp, instrument) DO UPDATE SET
                    open = EXCLUDED.open,
                    high = EXCLUDED.high,
                    low = EXCLUDED.low,
                    close = EXCLUDED.close,
                    volume = EXCLUDED.volume,
                    tick_count = EXCLUDED.tick_count
                """,
                [
                    (
                        bar.timestamp, bar.instrument,
                        bar.open, bar.high, bar.low, bar.close,
                        bar.volume, bar.tick_count
                    )
                    for bar in bars
                ]
            )

        logger.info(f"Stored {len(bars)} bars to {table_name}")

    async def get_bars(
        self,
        instrument: str,
        start_date: datetime,
        end_date: datetime,
        timeframe: TimeFrame
    ) -> List[Bar]:
        """Retrieve bars from database"""
        table_name = f"bars_{timeframe.value}"

        async with self.db_pool.acquire() as conn:
            rows = await conn.fetch(
                f"""
                SELECT timestamp, instrument, open, high, low, close, volume, tick_count
                FROM {table_name}
                WHERE instrument = $1
                  AND timestamp >= $2
                  AND timestamp <= $3
                ORDER BY timestamp
                """,
                instrument, start_date, end_date
            )

        return [
            Bar(
                timestamp=row["timestamp"],
                instrument=row["instrument"],
                timeframe=timeframe,
                open=Decimal(str(row["open"])),
                high=Decimal(str(row["high"])),
                low=Decimal(str(row["low"])),
                close=Decimal(str(row["close"])),
                volume=Decimal(str(row["volume"])),
                tick_count=row["tick_count"]
            )
            for row in rows
        ]

    async def store_ticks(self, ticks: List[Tick]) -> None:
        """Store ticks in database"""
        if not ticks:
            return

        async with self.db_pool.acquire() as conn:
            await conn.executemany(
                """
                INSERT INTO ticks
                (timestamp, instrument, bid, ask, bid_size, ask_size, last_trade_price, last_trade_size)
                VALUES ($1, $2, $3, $4, $5, $6, $7, $8)
                ON CONFLICT (timestamp, instrument) DO NOTHING
                """,
                [
                    (
                        tick.timestamp, tick.instrument,
                        tick.bid, tick.ask,
                        tick.bid_size, tick.ask_size,
                        tick.last_trade_price, tick.last_trade_size
                    )
                    for tick in ticks
                ]
            )

        logger.info(f"Stored {len(ticks)} ticks")

    async def get_ticks(
        self,
        instrument: str,
        start_date: datetime,
        end_date: datetime
    ) -> List[Tick]:
        """Retrieve ticks from database"""
        async with self.db_pool.acquire() as conn:
            rows = await conn.fetch(
                """
                SELECT timestamp, instrument, bid, ask, bid_size, ask_size,
                       last_trade_price, last_trade_size
                FROM ticks
                WHERE instrument = $1
                  AND timestamp >= $2
                  AND timestamp <= $3
                ORDER BY timestamp
                """,
                instrument, start_date, end_date
            )

        return [
            Tick(
                timestamp=row["timestamp"],
                instrument=row["instrument"],
                bid=Decimal(str(row["bid"])),
                ask=Decimal(str(row["ask"])),
                bid_size=Decimal(str(row["bid_size"])),
                ask_size=Decimal(str(row["ask_size"])),
                last_trade_price=Decimal(str(row["last_trade_price"])) if row["last_trade_price"] else None,
                last_trade_size=Decimal(str(row["last_trade_size"])) if row["last_trade_size"] else None
            )
            for row in rows
        ]
