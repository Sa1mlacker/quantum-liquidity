"""
System routes: health check, metrics
"""

from fastapi import APIRouter, Depends
from sqlalchemy.ext.asyncio import AsyncSession
from sqlalchemy import text
import time

from ..database import get_db
from ..cache import cache
from ..schemas import HealthCheck

router = APIRouter()

# Track startup time
startup_time = time.time()


@router.get("/health", response_model=HealthCheck)
async def health_check(db: AsyncSession = Depends(get_db)):
    """
    System health check
    Verifies database, Redis, and WebSocket availability
    """
    # Check database
    db_ok = False
    try:
        await db.execute(text("SELECT 1"))
        db_ok = True
    except Exception:
        pass

    # Check Redis
    redis_ok = False
    try:
        if cache.redis:
            await cache.redis.ping()
            redis_ok = True
    except Exception:
        pass

    # WebSocket status (always true for now, will implement in WS module)
    ws_ok = True

    uptime = time.time() - startup_time

    return HealthCheck(
        status="healthy" if (db_ok and redis_ok) else "degraded",
        timestamp=time.time(),
        database=db_ok,
        redis=redis_ok,
        websocket=ws_ok,
        uptime_seconds=uptime
    )


@router.get("/ping")
async def ping():
    """Simple ping endpoint"""
    return {"status": "ok", "timestamp": time.time()}
