"""Market Data API routes"""

from fastapi import APIRouter, Depends, Query
from sqlalchemy.ext.asyncio import AsyncSession
from typing import List

from ..database import get_db, DatabaseService
from ..cache import cached
from ..schemas import Tick, OHLCV

router = APIRouter()


@router.get("/ticks/{instrument}", response_model=List[Tick])
@cached("market:ticks", ttl=1)
async def get_ticks(
    instrument: str,
    limit: int = Query(100, le=1000),
    db: AsyncSession = Depends(get_db)
):
    """Get recent ticks"""
    db_service = DatabaseService(db)
    rows = await db_service.get_latest_ticks(instrument, limit)
    return [Tick(**dict(row._mapping)) for row in rows]


@router.get("/bars/{instrument}", response_model=List[OHLCV])
@cached("market:bars", ttl=1)
async def get_bars(
    instrument: str,
    timeframe: str = Query("1m", regex="^(1m|5m|15m|1h|4h|1d)$"),
    limit: int = Query(500, le=5000),
    db: AsyncSession = Depends(get_db)
):
    """Get OHLCV bars"""
    db_service = DatabaseService(db)
    rows = await db_service.get_bars(instrument, timeframe, limit)
    return [OHLCV(**dict(row._mapping)) for row in rows]
