"""
QuantumLiquidity REST API
Optimized for desktop app with low latency
"""

from fastapi import FastAPI, Request
from fastapi.middleware.cors import CORSMiddleware
from fastapi.middleware.gzip import GZipMiddleware
from fastapi.responses import ORJSONResponse
from contextlib import asynccontextmanager
import logging
import time

from .config import settings
from .database import init_db, close_db
from .cache import cache
from .routes import (
    orders,
    positions,
    trades,
    risk,
    analytics,
    market_data,
    sentiment,
    system,
)
from .websocket.stream import websocket_endpoint

# Setup logging
logging.basicConfig(
    level=settings.log_level,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


# ==================== Lifespan ====================

@asynccontextmanager
async def lifespan(app: FastAPI):
    """
    Application lifespan
    Initialize/cleanup resources
    """
    # Startup
    logger.info("Starting QuantumLiquidity API...")
    logger.info("Configuration: host=%s, port=%s", settings.api_host, settings.api_port)

    try:
        await init_db()
        await cache.connect()
        logger.info("API ready")
    except Exception as e:
        logger.error("Startup failed: %s", e)
        raise

    yield

    # Shutdown
    logger.info("Shutting down API...")
    await close_db()
    await cache.close()
    logger.info("API stopped")


# ==================== App ====================

app = FastAPI(
    title="QuantumLiquidity API",
    version="1.0.0",
    description="High-performance trading API with binary serialization",
    lifespan=lifespan,
    default_response_class=ORJSONResponse,  # Fast JSON serialization
    docs_url="/docs",
    redoc_url="/redoc",
)


# ==================== Middleware ====================

# CORS for Tauri desktop app
app.add_middleware(
    CORSMiddleware,
    allow_origins=settings.cors_origins,
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Gzip compression
if settings.enable_compression:
    app.add_middleware(GZipMiddleware, minimum_size=1000)


# Request timing middleware
@app.middleware("http")
async def add_process_time_header(request: Request, call_next):
    """Add X-Process-Time header to responses"""
    start_time = time.time()
    response = await call_next(request)
    process_time = time.time() - start_time
    response.headers["X-Process-Time"] = f"{process_time:.3f}"
    return response


# ==================== Routes ====================

# Health check
app.include_router(
    system.router,
    prefix="/api/v1",
    tags=["System"]
)

# Trading
app.include_router(
    orders.router,
    prefix="/api/v1/orders",
    tags=["Orders"]
)

app.include_router(
    positions.router,
    prefix="/api/v1/positions",
    tags=["Positions"]
)

app.include_router(
    trades.router,
    prefix="/api/v1/trades",
    tags=["Trades"]
)

# Risk
app.include_router(
    risk.router,
    prefix="/api/v1/risk",
    tags=["Risk"]
)

# Analytics
app.include_router(
    analytics.router,
    prefix="/api/v1/analytics",
    tags=["Analytics"]
)

# Market Data
app.include_router(
    market_data.router,
    prefix="/api/v1/market",
    tags=["Market Data"]
)

# Sentiment
app.include_router(
    sentiment.router,
    prefix="/api/v1/sentiment",
    tags=["Sentiment"]
)


# ==================== WebSocket ====================

@app.websocket("/ws/stream")
async def websocket_route(websocket):
    """WebSocket streaming endpoint with MessagePack"""
    await websocket_endpoint(websocket)


# ==================== Root ====================

@app.get("/")
async def root():
    """API root"""
    return {
        "name": "QuantumLiquidity API",
        "version": "1.0.0",
        "status": "operational",
        "docs": "/docs",
        "health": "/api/v1/health"
    }


# ==================== Error Handlers ====================

@app.exception_handler(404)
async def not_found_handler(request: Request, exc):
    """Handle 404 errors"""
    return ORJSONResponse(
        status_code=404,
        content={
            "error": "Not Found",
            "detail": f"Path {request.url.path} not found",
            "timestamp": time.time()
        }
    )


@app.exception_handler(500)
async def internal_error_handler(request: Request, exc):
    """Handle 500 errors"""
    logger.error("Internal error: %s", exc)
    return ORJSONResponse(
        status_code=500,
        content={
            "error": "Internal Server Error",
            "detail": "An unexpected error occurred",
            "timestamp": time.time()
        }
    )


if __name__ == "__main__":
    import uvicorn

    uvicorn.run(
        "main:app",
        host=settings.api_host,
        port=settings.api_port,
        reload=settings.api_reload,
        workers=settings.api_workers,
        log_level=settings.log_level.lower(),
    )
