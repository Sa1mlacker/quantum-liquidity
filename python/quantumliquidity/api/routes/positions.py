"""Positions API routes"""

from fastapi import APIRouter, Depends
from sqlalchemy.ext.asyncio import AsyncSession
from typing import List

from ..database import get_db, DatabaseService
from ..cache import cached
from ..schemas import PositionResponse, PositionSummary

router = APIRouter()


@router.get("", response_model=List[PositionResponse])
@cached("positions", ttl=1)
async def get_positions(db: AsyncSession = Depends(get_db)):
    """Get all current positions (cached 1sec)"""
    db_service = DatabaseService(db)
    rows = await db_service.get_latest_positions()
    return [PositionResponse(**dict(row._mapping)) for row in rows]


@router.get("/summary", response_model=PositionSummary)
@cached("positions:summary", ttl=1)
async def get_positions_summary(db: AsyncSession = Depends(get_db)):
    """Get portfolio summary"""
    db_service = DatabaseService(db)
    rows = await db_service.get_latest_positions()
    positions = [PositionResponse(**dict(row._mapping)) for row in rows]

    total_exposure = sum(abs(p.quantity * p.entry_price) for p in positions)
    total_unrealized = sum(p.unrealized_pnl for p in positions)
    total_realized = sum(p.realized_pnl for p in positions)
    total_commission = sum(p.total_commission for p in positions)

    return PositionSummary(
        total_positions=len(positions),
        total_exposure=total_exposure,
        total_unrealized_pnl=total_unrealized,
        total_realized_pnl=total_realized,
        total_commission=total_commission,
        positions=positions
    )


@router.get("/{instrument}", response_model=PositionResponse)
@cached("position", ttl=1)
async def get_position(instrument: str, db: AsyncSession = Depends(get_db)):
    """Get position for specific instrument"""
    db_service = DatabaseService(db)
    row = await db_service.get_position(instrument)
    if row:
        return PositionResponse(**dict(row._mapping))
    return PositionResponse(
        instrument=instrument,
        quantity=0, entry_price=0, unrealized_pnl=0,
        realized_pnl=0, num_fills_today=0, total_commission=0,
        last_update_ns=0
    )
