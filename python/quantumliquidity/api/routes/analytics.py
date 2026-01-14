"""Analytics API routes"""

from fastapi import APIRouter, Depends, Query
from sqlalchemy.ext.asyncio import AsyncSession
from typing import List

from ..database import get_db, DatabaseService
from ..cache import cached
from ..schemas import DailyStats, ORBStats, VolumeProfile

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
