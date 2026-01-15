"""Strategy management API routes"""

from fastapi import APIRouter, HTTPException
from pydantic import BaseModel
from typing import List, Optional
from enum import Enum

router = APIRouter()


class StrategyState(str, Enum):
    IDLE = "IDLE"
    RUNNING = "RUNNING"
    STOPPED = "STOPPED"
    ERROR = "ERROR"


class StrategyType(str, Enum):
    ORB = "ORB"
    MEAN_REVERSION = "MEAN_REVERSION"
    TREND_FOLLOWING = "TREND_FOLLOWING"


class StrategyConfig(BaseModel):
    name: str
    type: StrategyType
    instruments: List[str]
    max_position_size: float
    max_daily_loss: float
    enabled: bool = True

    # ORB specific
    orb_period_minutes: Optional[int] = 30
    orb_breakout_threshold: Optional[float] = 0.25


class StrategyStatus(BaseModel):
    name: str
    type: StrategyType
    state: StrategyState
    instruments: List[str]
    current_pnl: float
    daily_pnl: float
    num_trades_today: int
    enabled: bool


class StrategyPerformance(BaseModel):
    name: str
    total_trades: int
    winning_trades: int
    losing_trades: int
    win_rate: float
    total_pnl: float
    avg_win: float
    avg_loss: float
    profit_factor: float
    sharpe_ratio: Optional[float] = None


# In-memory strategy registry (in production, use database)
strategies_registry = {}


@router.get("/", response_model=List[StrategyStatus])
async def list_strategies():
    """List all registered strategies"""
    return [
        StrategyStatus(
            name=name,
            type=config["type"],
            state=config["state"],
            instruments=config["instruments"],
            current_pnl=config.get("current_pnl", 0.0),
            daily_pnl=config.get("daily_pnl", 0.0),
            num_trades_today=config.get("num_trades_today", 0),
            enabled=config["enabled"],
        )
        for name, config in strategies_registry.items()
    ]


@router.get("/{strategy_name}", response_model=StrategyStatus)
async def get_strategy(strategy_name: str):
    """Get specific strategy status"""
    if strategy_name not in strategies_registry:
        raise HTTPException(status_code=404, detail="Strategy not found")

    config = strategies_registry[strategy_name]
    return StrategyStatus(
        name=strategy_name,
        type=config["type"],
        state=config["state"],
        instruments=config["instruments"],
        current_pnl=config.get("current_pnl", 0.0),
        daily_pnl=config.get("daily_pnl", 0.0),
        num_trades_today=config.get("num_trades_today", 0),
        enabled=config["enabled"],
    )


@router.post("/", response_model=dict)
async def create_strategy(config: StrategyConfig):
    """Create new strategy"""
    if config.name in strategies_registry:
        raise HTTPException(status_code=400, detail="Strategy already exists")

    strategies_registry[config.name] = {
        "type": config.type,
        "state": StrategyState.IDLE,
        "instruments": config.instruments,
        "max_position_size": config.max_position_size,
        "max_daily_loss": config.max_daily_loss,
        "enabled": config.enabled,
        "orb_period_minutes": config.orb_period_minutes,
        "orb_breakout_threshold": config.orb_breakout_threshold,
        "current_pnl": 0.0,
        "daily_pnl": 0.0,
        "num_trades_today": 0,
    }

    return {"status": "created", "name": config.name}


@router.post("/{strategy_name}/start")
async def start_strategy(strategy_name: str):
    """Start a strategy"""
    if strategy_name not in strategies_registry:
        raise HTTPException(status_code=404, detail="Strategy not found")

    config = strategies_registry[strategy_name]

    if config["state"] == StrategyState.RUNNING:
        raise HTTPException(status_code=400, detail="Strategy already running")

    if not config["enabled"]:
        raise HTTPException(status_code=400, detail="Strategy is disabled")

    # In production, this would call C++ strategy manager
    config["state"] = StrategyState.RUNNING

    return {"status": "started", "name": strategy_name}


@router.post("/{strategy_name}/stop")
async def stop_strategy(strategy_name: str):
    """Stop a strategy"""
    if strategy_name not in strategies_registry:
        raise HTTPException(status_code=404, detail="Strategy not found")

    config = strategies_registry[strategy_name]

    if config["state"] != StrategyState.RUNNING:
        raise HTTPException(status_code=400, detail="Strategy not running")

    # In production, this would call C++ strategy manager
    config["state"] = StrategyState.STOPPED

    return {"status": "stopped", "name": strategy_name}


@router.delete("/{strategy_name}")
async def delete_strategy(strategy_name: str):
    """Delete a strategy"""
    if strategy_name not in strategies_registry:
        raise HTTPException(status_code=404, detail="Strategy not found")

    config = strategies_registry[strategy_name]

    if config["state"] == StrategyState.RUNNING:
        raise HTTPException(
            status_code=400, detail="Cannot delete running strategy. Stop it first."
        )

    del strategies_registry[strategy_name]

    return {"status": "deleted", "name": strategy_name}


@router.get("/{strategy_name}/performance", response_model=StrategyPerformance)
async def get_strategy_performance(strategy_name: str, days: int = 30):
    """Get strategy performance metrics"""
    if strategy_name not in strategies_registry:
        raise HTTPException(status_code=404, detail="Strategy not found")

    # In production, query from database
    # For now, return mock data
    return StrategyPerformance(
        name=strategy_name,
        total_trades=150,
        winning_trades=95,
        losing_trades=55,
        win_rate=63.3,
        total_pnl=12500.50,
        avg_win=250.00,
        avg_loss=-125.00,
        profit_factor=1.89,
        sharpe_ratio=1.45,
    )


@router.post("/start-all")
async def start_all_strategies():
    """Start all enabled strategies"""
    started = []

    for name, config in strategies_registry.items():
        if config["enabled"] and config["state"] != StrategyState.RUNNING:
            config["state"] = StrategyState.RUNNING
            started.append(name)

    return {"status": "started", "strategies": started, "count": len(started)}


@router.post("/stop-all")
async def stop_all_strategies():
    """Stop all running strategies"""
    stopped = []

    for name, config in strategies_registry.items():
        if config["state"] == StrategyState.RUNNING:
            config["state"] = StrategyState.STOPPED
            stopped.append(name)

    return {"status": "stopped", "strategies": stopped, "count": len(stopped)}
