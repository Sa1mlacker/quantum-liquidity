"""Trades API routes"""

from fastapi import APIRouter, Depends, Query
from sqlalchemy.ext.asyncio import AsyncSession
from typing import Optional

from ..database import get_db, DatabaseService
from ..schemas import TradeHistory, TradeResponse

router = APIRouter()


@router.get("", response_model=TradeHistory)
async def get_trades(
    limit: int = Query(100, le=500),
    offset: int = Query(0, ge=0),
    instrument: Optional[str] = None,
    from_timestamp: Optional[int] = None,
    db: AsyncSession = Depends(get_db)
):
    """Get trade history with pagination"""
    db_service = DatabaseService(db)

    rows = await db_service.get_trades(
        limit=limit,
        offset=offset,
        instrument=instrument,
        from_timestamp=from_timestamp
    )
    total = await db_service.get_trade_count(instrument)

    trades = [TradeResponse(**dict(row._mapping)) for row in rows]

    return TradeHistory(
        total=total,
        page=offset // limit + 1,
        page_size=limit,
        trades=trades
    )
