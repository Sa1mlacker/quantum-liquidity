"""Risk API routes"""

from fastapi import APIRouter, Depends, HTTPException
from sqlalchemy.ext.asyncio import AsyncSession

from ..database import get_db, DatabaseService
from ..cache import cached
from ..schemas import RiskMetrics

router = APIRouter()


@router.get("/metrics", response_model=RiskMetrics)
@cached("risk:metrics", ttl=1)
async def get_risk_metrics(db: AsyncSession = Depends(get_db)):
    """
    Get current risk metrics
    Cached for 1 second
    """
    db_service = DatabaseService(db)
    row = await db_service.get_latest_risk_metrics()

    if not row:
        raise HTTPException(status_code=404, detail="No risk metrics available")

    return RiskMetrics(**dict(row._mapping))
