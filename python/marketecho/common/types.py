"""Common type definitions matching C++ types"""

from datetime import datetime
from decimal import Decimal
from enum import Enum
from typing import Optional
from pydantic import BaseModel, Field


class Side(str, Enum):
    BUY = "BUY"
    SELL = "SELL"


class OrderType(str, Enum):
    MARKET = "MARKET"
    LIMIT = "LIMIT"
    STOP = "STOP"
    STOP_LIMIT = "STOP_LIMIT"


class OrderState(str, Enum):
    PENDING_SUBMIT = "PENDING_SUBMIT"
    SUBMITTED = "SUBMITTED"
    ACCEPTED = "ACCEPTED"
    PARTIALLY_FILLED = "PARTIALLY_FILLED"
    FILLED = "FILLED"
    PENDING_CANCEL = "PENDING_CANCEL"
    CANCELLED = "CANCELLED"
    REJECTED = "REJECTED"
    EXPIRED = "EXPIRED"


class TimeInForce(str, Enum):
    GTC = "GTC"
    IOC = "IOC"
    FOK = "FOK"
    DAY = "DAY"


class TimeFrame(str, Enum):
    TICK = "TICK"
    SEC_1 = "1s"
    SEC_5 = "5s"
    SEC_15 = "15s"
    SEC_30 = "30s"
    MIN_1 = "1m"
    MIN_5 = "5m"
    MIN_15 = "15m"
    MIN_30 = "30m"
    HOUR_1 = "1h"
    HOUR_4 = "4h"
    DAY_1 = "1d"


class Tick(BaseModel):
    timestamp: datetime
    instrument: str
    bid: Decimal
    ask: Decimal
    bid_size: Decimal
    ask_size: Decimal
    last_trade_price: Optional[Decimal] = None
    last_trade_size: Optional[Decimal] = None


class Bar(BaseModel):
    timestamp: datetime
    instrument: str
    timeframe: TimeFrame
    open: Decimal
    high: Decimal
    low: Decimal
    close: Decimal
    volume: Decimal
    tick_count: int = 0


class Position(BaseModel):
    instrument: str
    quantity: Decimal  # Positive = long, negative = short
    average_price: Decimal
    unrealized_pnl: Decimal = Decimal("0")
    realized_pnl: Decimal = Decimal("0")
    last_update: datetime


class OrderRequest(BaseModel):
    strategy_id: str
    instrument: str
    side: Side
    order_type: OrderType
    quantity: Decimal
    limit_price: Optional[Decimal] = None
    stop_price: Optional[Decimal] = None
    time_in_force: TimeInForce = TimeInForce.GTC
    client_order_id: str


class OrderUpdate(BaseModel):
    order_id: int
    client_order_id: str
    state: OrderState
    timestamp: datetime
    reject_reason: Optional[str] = None
    filled_quantity: Decimal = Decimal("0")
    remaining_quantity: Decimal
    average_fill_price: Optional[Decimal] = None


class Fill(BaseModel):
    order_id: int
    timestamp: datetime
    instrument: str
    side: Side
    quantity: Decimal
    price: Decimal
    execution_id: Optional[str] = None
    commission: Optional[Decimal] = None
