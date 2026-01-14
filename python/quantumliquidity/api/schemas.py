"""
Pydantic schemas for API request/response
Optimized for binary serialization with msgpack
"""

from pydantic import BaseModel, Field, ConfigDict
from typing import Optional, List
from datetime import datetime
from enum import Enum


# ==================== Enums ====================

class OrderSide(str, Enum):
    BUY = "BUY"
    SELL = "SELL"


class OrderType(str, Enum):
    MARKET = "MARKET"
    LIMIT = "LIMIT"
    STOP = "STOP"
    STOP_LIMIT = "STOP_LIMIT"


class OrderStatus(str, Enum):
    PENDING = "PENDING"
    SUBMITTED = "SUBMITTED"
    ACKNOWLEDGED = "ACKNOWLEDGED"
    PARTIALLY_FILLED = "PARTIALLY_FILLED"
    FILLED = "FILLED"
    CANCELLED = "CANCELLED"
    REJECTED = "REJECTED"
    ERROR = "ERROR"
    EXPIRED = "EXPIRED"


class TimeInForce(str, Enum):
    DAY = "DAY"
    GTC = "GTC"
    IOC = "IOC"
    FOK = "FOK"


# ==================== Base Models ====================

class BaseSchema(BaseModel):
    """Base schema with optimized config"""
    model_config = ConfigDict(
        from_attributes=True,  # Allow ORM mode
        use_enum_values=True,  # Serialize enums as values
        json_encoders={
            datetime: lambda v: int(v.timestamp() * 1e9)  # Nanosecond timestamps
        }
    )


# ==================== Order Schemas ====================

class OrderRequest(BaseSchema):
    """Order submission request"""
    instrument: str = Field(..., max_length=32)
    side: OrderSide
    order_type: OrderType
    quantity: float = Field(..., gt=0)
    price: Optional[float] = Field(None, gt=0)
    stop_price: Optional[float] = Field(None, gt=0)
    time_in_force: TimeInForce = TimeInForce.GTC
    strategy_id: str = Field(..., max_length=64)
    user_comment: Optional[str] = Field(None, max_length=256)


class OrderResponse(BaseSchema):
    """Order status response"""
    order_id: int
    client_order_id: str
    instrument: str
    side: OrderSide
    order_type: OrderType
    quantity: float
    price: Optional[float]
    status: OrderStatus
    filled_quantity: float
    remaining_quantity: float
    average_fill_price: Optional[float]
    reject_reason: Optional[str]
    created_at: datetime
    submitted_at: Optional[datetime]
    filled_at: Optional[datetime]
    provider: Optional[str]


# ==================== Position Schemas ====================

class PositionResponse(BaseSchema):
    """Current position"""
    instrument: str
    quantity: float  # Signed: + = long, - = short
    entry_price: float
    unrealized_pnl: float
    realized_pnl: float
    num_fills_today: int
    total_commission: float
    last_update_ns: int


class PositionSummary(BaseSchema):
    """Portfolio summary"""
    total_positions: int
    total_exposure: float
    total_unrealized_pnl: float
    total_realized_pnl: float
    total_commission: float
    positions: List[PositionResponse]


# ==================== Trade Schemas ====================

class TradeResponse(BaseSchema):
    """Trade (fill) record"""
    trade_id: int
    fill_id: str
    order_id: int
    client_order_id: str
    instrument: str
    side: OrderSide
    quantity: float
    price: float
    commission: float
    pnl: Optional[float]
    timestamp: datetime
    provider: Optional[str]


class TradeHistory(BaseSchema):
    """Paginated trade history"""
    total: int
    page: int
    page_size: int
    trades: List[TradeResponse]


# ==================== Risk Schemas ====================

class RiskMetrics(BaseSchema):
    """Current risk metrics"""
    timestamp: datetime
    total_exposure: float
    account_utilization: float  # Percentage
    daily_pnl: float
    realized_pnl: float
    unrealized_pnl: float
    max_dd_today: float
    daily_high_pnl: float
    orders_submitted_today: int
    orders_filled_today: int
    orders_rejected_today: int
    orders_cancelled_today: int
    halt_active: bool
    halt_reason: Optional[str]


# ==================== Analytics Schemas ====================

class DayType(str, Enum):
    TREND = "TREND"
    RANGE = "RANGE"
    V_DAY = "V_DAY"
    P_DAY = "P_DAY"
    NORMAL = "NORMAL"


class DailyStats(BaseSchema):
    """Daily market statistics"""
    date: datetime
    instrument: str
    day_type: Optional[DayType]
    open_price: Optional[float]
    high_price: Optional[float]
    low_price: Optional[float]
    close_price: Optional[float]
    range_points: Optional[float]
    atr: Optional[float]
    trend_strength: Optional[float]


class ORBStats(BaseSchema):
    """Opening Range Breakout statistics"""
    date: datetime
    instrument: str
    or_period_minutes: int
    or_high: Optional[float]
    or_low: Optional[float]
    or_size: Optional[float]
    breakout_occurred: Optional[bool]
    breakout_direction: Optional[str]
    breakout_time: Optional[datetime]
    max_r_multiple_up: Optional[float]
    max_r_multiple_down: Optional[float]


class VolumeProfile(BaseSchema):
    """Volume profile (VAH/VAL/POC)"""
    date: datetime
    instrument: str
    value_area_high: Optional[float]
    value_area_low: Optional[float]
    point_of_control: Optional[float]
    profile_type: Optional[str]


# ==================== Market Data Schemas ====================

class Tick(BaseSchema):
    """Market tick data"""
    timestamp: datetime
    instrument: str
    bid: float
    ask: float
    bid_size: Optional[float]
    ask_size: Optional[float]
    last_trade_price: Optional[float]
    last_trade_size: Optional[float]
    provider: Optional[str]

    @property
    def mid(self) -> float:
        """Mid price"""
        return (self.bid + self.ask) / 2.0


class OHLCV(BaseSchema):
    """OHLCV bar"""
    timestamp: datetime
    instrument: str
    open: float
    high: float
    low: float
    close: float
    volume: float
    tick_count: int
    provider: Optional[str]


# ==================== Sentiment Schemas ====================

class Sentiment(str, Enum):
    VERY_BEARISH = "VERY_BEARISH"
    BEARISH = "BEARISH"
    NEUTRAL = "NEUTRAL"
    BULLISH = "BULLISH"
    VERY_BULLISH = "VERY_BULLISH"


class NewsEvent(BaseSchema):
    """News event"""
    id: str
    timestamp: datetime
    title: str
    content: Optional[str]
    source: Optional[str]
    instruments: List[str]
    sentiment: Optional[Sentiment]
    event_type: Optional[str]
    confidence: Optional[float]


class SentimentSeries(BaseSchema):
    """Aggregated sentiment time series"""
    timestamp: datetime
    instrument: str
    sentiment_score: float  # -1.0 to +1.0
    event_count: int
    avg_confidence: Optional[float]


# ==================== Strategy Schemas ====================

class StrategyStatus(str, Enum):
    STOPPED = "STOPPED"
    RUNNING = "RUNNING"
    PAUSED = "PAUSED"
    ERROR = "ERROR"


class StrategyInfo(BaseSchema):
    """Strategy information"""
    strategy_id: str
    name: str
    status: StrategyStatus
    instruments: List[str]
    total_pnl: float
    daily_pnl: float
    win_rate: Optional[float]
    sharpe_ratio: Optional[float]
    max_drawdown: Optional[float]
    trade_count: int


# ==================== System Schemas ====================

class HealthCheck(BaseSchema):
    """System health check"""
    status: str
    timestamp: datetime
    database: bool
    redis: bool
    websocket: bool
    uptime_seconds: float


class ErrorResponse(BaseSchema):
    """Error response"""
    error: str
    detail: Optional[str]
    timestamp: datetime
