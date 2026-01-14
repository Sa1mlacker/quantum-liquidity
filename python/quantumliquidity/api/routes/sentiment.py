"""Sentiment API routes"""

from fastapi import APIRouter, Depends, Query
from sqlalchemy.ext.asyncio import AsyncSession
from typing import Optional, List

from ..database import get_db, DatabaseService
from ..cache import cached
from ..schemas import NewsEvent, SentimentSeries

router = APIRouter()


@router.get("/news", response_model=List[NewsEvent])
@cached("sentiment:news", ttl=10)
async def get_news(
    instrument: Optional[str] = None,
    limit: int = Query(100, le=500),
    db: AsyncSession = Depends(get_db)
):
    """Get recent news events"""
    db_service = DatabaseService(db)
    rows = await db_service.get_news_events(instrument, limit)
    return [NewsEvent(**dict(row._mapping)) for row in rows]


@router.get("/series/{instrument}", response_model=List[SentimentSeries])
@cached("sentiment:series", ttl=5)
async def get_sentiment_series(
    instrument: str,
    limit: int = Query(100, le=500),
    db: AsyncSession = Depends(get_db)
):
    """Get sentiment time series"""
    db_service = DatabaseService(db)
    rows = await db_service.get_sentiment_series(instrument, limit)
    return [SentimentSeries(**dict(row._mapping)) for row in rows]
