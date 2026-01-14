"""
Orders API routes
"""

from fastapi import APIRouter, Depends, HTTPException, Query
from sqlalchemy.ext.asyncio import AsyncSession
from typing import Optional, List

from ..database import get_db, DatabaseService
from ..cache import cache, CacheKeys, cached
from ..schemas import OrderResponse

router = APIRouter()


@router.get("", response_model=List[OrderResponse])
@cached("orders", ttl=1)
async def get_orders(
    limit: int = Query(100, le=500, description="Max 500 orders"),
    offset: int = Query(0, ge=0),
    status: Optional[str] = Query(None, description="Filter by status"),
    instrument: Optional[str] = Query(None, description="Filter by instrument"),
    db: AsyncSession = Depends(get_db)
):
    """
    Get orders with pagination and filters
    Cached for 1 second
    """
    db_service = DatabaseService(db)
    rows = await db_service.get_orders(
        limit=limit,
        offset=offset,
        status=status,
        instrument=instrument
    )

    return [OrderResponse(**dict(row._mapping)) for row in rows]


@router.get("/{order_id}", response_model=OrderResponse)
async def get_order(
    order_id: int,
    db: AsyncSession = Depends(get_db)
):
    """Get single order by ID"""
    db_service = DatabaseService(db)
    row = await db_service.get_order_by_id(order_id)

    if not row:
        raise HTTPException(status_code=404, detail="Order not found")

    return OrderResponse(**dict(row._mapping))


@router.get("/active/count")
@cached("orders:active:count", ttl=1)
async def get_active_orders_count(
    db: AsyncSession = Depends(get_db)
):
    """Get count of active orders"""
    db_service = DatabaseService(db)
    rows = await db_service.get_orders(
        limit=10000,
        status="ACKNOWLEDGED"
    )
    return {"count": len(rows)}
