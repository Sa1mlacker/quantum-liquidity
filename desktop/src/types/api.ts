/**
 * API Type Definitions
 * Mirror of Python Pydantic schemas
 */

export enum OrderSide {
  BUY = 'BUY',
  SELL = 'SELL',
}

export enum OrderStatus {
  PENDING = 'PENDING',
  SUBMITTED = 'SUBMITTED',
  ACKNOWLEDGED = 'ACKNOWLEDGED',
  PARTIALLY_FILLED = 'PARTIALLY_FILLED',
  FILLED = 'FILLED',
  CANCELLED = 'CANCELLED',
  REJECTED = 'REJECTED',
  ERROR = 'ERROR',
  EXPIRED = 'EXPIRED',
}

export interface Position {
  instrument: string
  quantity: number  // Signed: + = long, - = short
  entry_price: number
  unrealized_pnl: number
  realized_pnl: number
  num_fills_today: number
  total_commission: number
  last_update_ns: number
}

export interface PositionSummary {
  total_positions: number
  total_exposure: number
  total_unrealized_pnl: number
  total_realized_pnl: number
  total_commission: number
  positions: Position[]
}

export interface Trade {
  trade_id: number
  fill_id: string
  order_id: number
  client_order_id: string
  instrument: string
  side: OrderSide
  quantity: number
  price: number
  commission: number
  pnl: number | null
  timestamp: string
  provider: string | null
}

export interface RiskMetrics {
  timestamp: string
  total_exposure: number
  account_utilization: number
  daily_pnl: number
  realized_pnl: number
  unrealized_pnl: number
  max_dd_today: number
  daily_high_pnl: number
  orders_submitted_today: number
  orders_filled_today: number
  orders_rejected_today: number
  orders_cancelled_today: number
  halt_active: boolean
  halt_reason: string | null
}

export interface OHLCV {
  timestamp: string
  instrument: string
  open: number
  high: number
  low: number
  close: number
  volume: number
  tick_count: number
  provider: string | null
}

export interface HealthCheck {
  status: string
  timestamp: number
  database: boolean
  redis: boolean
  websocket: boolean
  uptime_seconds: number
}

// WebSocket message types
export enum WSMessageType {
  TICK = 'tick',
  BAR = 'bar',
  POSITION = 'position',
  ORDER = 'order',
  FILL = 'fill',
  RISK = 'risk',
  ACK = 'ack',
  ERROR = 'error',
}

export interface WSMessage {
  type: WSMessageType
  topic?: string
  timestamp?: number
  [key: string]: any
}
