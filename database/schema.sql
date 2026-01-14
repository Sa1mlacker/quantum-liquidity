-- MarketEcho-FX Database Schema
-- PostgreSQL with TimescaleDB extension for time-series optimization

-- Enable TimescaleDB extension
CREATE EXTENSION IF NOT EXISTS timescaledb;

-- ============================================================================
-- MARKET DATA TABLES
-- ============================================================================

-- Ticks table (raw tick data)
CREATE TABLE IF NOT EXISTS ticks (
    timestamp TIMESTAMPTZ NOT NULL,
    instrument VARCHAR(32) NOT NULL,
    bid NUMERIC(20, 8) NOT NULL,
    ask NUMERIC(20, 8) NOT NULL,
    bid_size NUMERIC(20, 4),
    ask_size NUMERIC(20, 4),
    last_trade_price NUMERIC(20, 8),
    last_trade_size NUMERIC(20, 4),
    provider VARCHAR(32),
    PRIMARY KEY (timestamp, instrument)
);

-- Convert to hypertable for time-series optimization
SELECT create_hypertable('ticks', 'timestamp', if_not_exists => TRUE);

-- Create indexes
CREATE INDEX IF NOT EXISTS idx_ticks_instrument ON ticks(instrument, timestamp DESC);

-- Bars tables (OHLCV by timeframe)
CREATE TABLE IF NOT EXISTS bars_1m (
    timestamp TIMESTAMPTZ NOT NULL,
    instrument VARCHAR(32) NOT NULL,
    open NUMERIC(20, 8) NOT NULL,
    high NUMERIC(20, 8) NOT NULL,
    low NUMERIC(20, 8) NOT NULL,
    close NUMERIC(20, 8) NOT NULL,
    volume NUMERIC(20, 4) DEFAULT 0,
    tick_count INTEGER DEFAULT 0,
    provider VARCHAR(32),
    PRIMARY KEY (timestamp, instrument)
);

SELECT create_hypertable('bars_1m', 'timestamp', if_not_exists => TRUE);
CREATE INDEX IF NOT EXISTS idx_bars_1m_instrument ON bars_1m(instrument, timestamp DESC);

-- Repeat for other timeframes (5m, 15m, 1h, 4h, 1d)
CREATE TABLE IF NOT EXISTS bars_5m (LIKE bars_1m INCLUDING ALL);
SELECT create_hypertable('bars_5m', 'timestamp', if_not_exists => TRUE);
CREATE INDEX IF NOT EXISTS idx_bars_5m_instrument ON bars_5m(instrument, timestamp DESC);

CREATE TABLE IF NOT EXISTS bars_15m (LIKE bars_1m INCLUDING ALL);
SELECT create_hypertable('bars_15m', 'timestamp', if_not_exists => TRUE);
CREATE INDEX IF NOT EXISTS idx_bars_15m_instrument ON bars_15m(instrument, timestamp DESC);

CREATE TABLE IF NOT EXISTS bars_1h (LIKE bars_1m INCLUDING ALL);
SELECT create_hypertable('bars_1h', 'timestamp', if_not_exists => TRUE);
CREATE INDEX IF NOT EXISTS idx_bars_1h_instrument ON bars_1h(instrument, timestamp DESC);

CREATE TABLE IF NOT EXISTS bars_4h (LIKE bars_1m INCLUDING ALL);
SELECT create_hypertable('bars_4h', 'timestamp', if_not_exists => TRUE);
CREATE INDEX IF NOT EXISTS idx_bars_4h_instrument ON bars_4h(instrument, timestamp DESC);

CREATE TABLE IF NOT EXISTS bars_1d (LIKE bars_1m INCLUDING ALL);
SELECT create_hypertable('bars_1d', 'timestamp', if_not_exists => TRUE);
CREATE INDEX IF NOT EXISTS idx_bars_1d_instrument ON bars_1d(instrument, timestamp DESC);

-- ============================================================================
-- ANALYTICS TABLES
-- ============================================================================

-- Daily statistics
CREATE TABLE IF NOT EXISTS daily_stats (
    date DATE NOT NULL,
    instrument VARCHAR(32) NOT NULL,
    day_type VARCHAR(32),  -- TREND, RANGE, V_DAY, P_DAY, NORMAL
    open_price NUMERIC(20, 8),
    high_price NUMERIC(20, 8),
    low_price NUMERIC(20, 8),
    close_price NUMERIC(20, 8),
    range_points NUMERIC(20, 8),
    atr NUMERIC(20, 8),
    trend_strength NUMERIC(5, 4),
    range_efficiency NUMERIC(5, 4),
    reversal_score NUMERIC(5, 4),
    created_at TIMESTAMPTZ DEFAULT NOW(),
    PRIMARY KEY (date, instrument)
);

CREATE INDEX IF NOT EXISTS idx_daily_stats_instrument ON daily_stats(instrument, date DESC);

-- Session statistics (Asia, London, NY)
CREATE TABLE IF NOT EXISTS session_stats (
    date DATE NOT NULL,
    instrument VARCHAR(32) NOT NULL,
    session VARCHAR(16) NOT NULL,  -- ASIA, LONDON, NY
    session_start TIMESTAMPTZ NOT NULL,
    session_end TIMESTAMPTZ NOT NULL,
    open_price NUMERIC(20, 8),
    high_price NUMERIC(20, 8),
    low_price NUMERIC(20, 8),
    close_price NUMERIC(20, 8),
    range_points NUMERIC(20, 8),
    avg_volume NUMERIC(20, 4),
    trend_direction VARCHAR(16),  -- UP, DOWN, NEUTRAL
    PRIMARY KEY (date, instrument, session)
);

CREATE INDEX IF NOT EXISTS idx_session_stats_instrument ON session_stats(instrument, date DESC);

-- ORB (Opening Range Breakout) statistics
CREATE TABLE IF NOT EXISTS orb_stats (
    date DATE NOT NULL,
    instrument VARCHAR(32) NOT NULL,
    or_period_minutes INTEGER NOT NULL,  -- e.g., 30, 60
    or_high NUMERIC(20, 8),
    or_low NUMERIC(20, 8),
    or_size NUMERIC(20, 8),
    breakout_occurred BOOLEAN,
    breakout_direction VARCHAR(16),  -- UP, DOWN, NONE
    breakout_time TIMESTAMPTZ,
    max_r_multiple_up NUMERIC(10, 2),  -- R-multiple on upside
    max_r_multiple_down NUMERIC(10, 2),  -- R-multiple on downside
    PRIMARY KEY (date, instrument, or_period_minutes)
);

CREATE INDEX IF NOT EXISTS idx_orb_stats_instrument ON orb_stats(instrument, date DESC);

-- Volume Profile (TPO/VAH/VAL/POC)
CREATE TABLE IF NOT EXISTS volume_profile (
    date DATE NOT NULL,
    instrument VARCHAR(32) NOT NULL,
    value_area_high NUMERIC(20, 8),  -- VAH
    value_area_low NUMERIC(20, 8),   -- VAL
    point_of_control NUMERIC(20, 8), -- POC (price with most volume)
    profile_type VARCHAR(16),  -- NORMAL, P_SHAPED, B_SHAPED
    PRIMARY KEY (date, instrument)
);

-- ============================================================================
-- SENTIMENT TABLES
-- ============================================================================

-- News events
CREATE TABLE IF NOT EXISTS news_events (
    id VARCHAR(64) PRIMARY KEY,
    timestamp TIMESTAMPTZ NOT NULL,
    title TEXT NOT NULL,
    content TEXT,
    source VARCHAR(128),
    instruments TEXT[],  -- Array of affected instruments
    sentiment VARCHAR(32),  -- VERY_BEARISH, BEARISH, NEUTRAL, BULLISH, VERY_BULLISH
    event_type VARCHAR(32),  -- MACRO, MONETARY_POLICY, GEOPOLITICAL, EARNINGS, OTHER
    confidence NUMERIC(3, 2),
    created_at TIMESTAMPTZ DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_news_timestamp ON news_events(timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_news_instruments ON news_events USING GIN(instruments);

-- Aggregated sentiment time series
CREATE TABLE IF NOT EXISTS sentiment_series (
    timestamp TIMESTAMPTZ NOT NULL,
    instrument VARCHAR(32) NOT NULL,
    sentiment_score NUMERIC(5, 4),  -- -1.0 (bearish) to +1.0 (bullish)
    event_count INTEGER,
    avg_confidence NUMERIC(3, 2),
    PRIMARY KEY (timestamp, instrument)
);

SELECT create_hypertable('sentiment_series', 'timestamp', if_not_exists => TRUE);
CREATE INDEX IF NOT EXISTS idx_sentiment_instrument ON sentiment_series(instrument, timestamp DESC);

-- ============================================================================
-- TRADING TABLES
-- ============================================================================

-- Orders
CREATE TABLE IF NOT EXISTS orders (
    order_id BIGSERIAL PRIMARY KEY,
    client_order_id VARCHAR(64) UNIQUE NOT NULL,
    strategy_id VARCHAR(64) NOT NULL,
    instrument VARCHAR(32) NOT NULL,
    side VARCHAR(8) NOT NULL,  -- BUY, SELL
    order_type VARCHAR(16) NOT NULL,  -- MARKET, LIMIT, STOP, STOP_LIMIT
    quantity NUMERIC(20, 4) NOT NULL,
    limit_price NUMERIC(20, 8),
    stop_price NUMERIC(20, 8),
    time_in_force VARCHAR(8) DEFAULT 'GTC',  -- DAY, GTC, IOC, FOK
    status VARCHAR(32) NOT NULL,  -- PENDING, SUBMITTED, ACKNOWLEDGED, PARTIALLY_FILLED, FILLED, CANCELLED, REJECTED, ERROR, EXPIRED
    filled_quantity NUMERIC(20, 4) DEFAULT 0,
    remaining_quantity NUMERIC(20, 4),
    average_fill_price NUMERIC(20, 8),
    reject_reason TEXT,
    created_at TIMESTAMPTZ DEFAULT NOW(),
    updated_at TIMESTAMPTZ DEFAULT NOW(),
    submitted_at TIMESTAMPTZ,
    filled_at TIMESTAMPTZ,
    exchange_order_id VARCHAR(128),
    provider VARCHAR(64),  -- Execution provider name (OANDA, InteractiveBrokers, etc.)
    user_comment TEXT
);

CREATE INDEX IF NOT EXISTS idx_orders_strategy ON orders(strategy_id, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_orders_instrument ON orders(instrument, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_orders_status ON orders(status, created_at DESC);
CREATE INDEX IF NOT EXISTS idx_orders_provider ON orders(provider, created_at DESC);

-- Trades (fills)
CREATE TABLE IF NOT EXISTS trades (
    trade_id BIGSERIAL PRIMARY KEY,
    fill_id VARCHAR(128) UNIQUE NOT NULL,  -- Unique fill identifier from provider
    order_id BIGINT NOT NULL REFERENCES orders(order_id),
    client_order_id VARCHAR(64) NOT NULL,  -- Reference to client order ID
    execution_id VARCHAR(128),  -- Exchange-specific execution ID
    timestamp TIMESTAMPTZ NOT NULL,
    instrument VARCHAR(32) NOT NULL,
    side VARCHAR(8) NOT NULL,  -- BUY, SELL
    quantity NUMERIC(20, 4) NOT NULL,
    price NUMERIC(20, 8) NOT NULL,
    commission NUMERIC(20, 8) DEFAULT 0,
    pnl NUMERIC(20, 8),  -- Realized P&L for this trade
    provider VARCHAR(64),  -- Execution provider
    created_at TIMESTAMPTZ DEFAULT NOW()
);

CREATE INDEX IF NOT EXISTS idx_trades_order ON trades(order_id);
CREATE INDEX IF NOT EXISTS idx_trades_client_order ON trades(client_order_id);
CREATE INDEX IF NOT EXISTS idx_trades_timestamp ON trades(timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_trades_instrument ON trades(instrument, timestamp DESC);

-- Positions (snapshots)
CREATE TABLE IF NOT EXISTS positions (
    timestamp TIMESTAMPTZ NOT NULL,
    instrument VARCHAR(32) NOT NULL,
    quantity NUMERIC(20, 4) NOT NULL,  -- Signed: positive = long, negative = short
    entry_price NUMERIC(20, 8) NOT NULL,  -- Weighted average entry price
    unrealized_pnl NUMERIC(20, 8),
    realized_pnl NUMERIC(20, 8),  -- Today's realized PnL
    num_fills_today INTEGER DEFAULT 0,
    total_commission NUMERIC(20, 8) DEFAULT 0,
    last_update_ns BIGINT,  -- Nanosecond timestamp of last update
    PRIMARY KEY (timestamp, instrument)
);

SELECT create_hypertable('positions', 'timestamp', if_not_exists => TRUE);

-- Risk metrics (snapshots)
CREATE TABLE IF NOT EXISTS risk_metrics (
    timestamp TIMESTAMPTZ NOT NULL PRIMARY KEY,
    total_exposure NUMERIC(20, 8),  -- Sum of |qty * price|
    account_utilization NUMERIC(5, 4),  -- % of bankroll used
    daily_pnl NUMERIC(20, 8),  -- Total PnL today
    realized_pnl NUMERIC(20, 8),  -- Closed trades
    unrealized_pnl NUMERIC(20, 8),  -- Open positions
    max_dd_today NUMERIC(20, 8),  -- Max drawdown from daily high
    daily_high_pnl NUMERIC(20, 8),  -- Highest PnL today
    orders_submitted_today INTEGER,
    orders_filled_today INTEGER,
    orders_rejected_today INTEGER,
    orders_cancelled_today INTEGER,
    halt_active BOOLEAN DEFAULT FALSE,
    halt_reason TEXT
);

SELECT create_hypertable('risk_metrics', 'timestamp', if_not_exists => TRUE);

-- Strategy performance metrics
CREATE TABLE IF NOT EXISTS strategy_metrics (
    timestamp TIMESTAMPTZ NOT NULL,
    strategy_id VARCHAR(64) NOT NULL,
    total_pnl NUMERIC(20, 8),
    daily_pnl NUMERIC(20, 8),
    win_rate NUMERIC(5, 4),
    sharpe_ratio NUMERIC(10, 4),
    max_drawdown NUMERIC(20, 8),
    trade_count INTEGER,
    PRIMARY KEY (timestamp, strategy_id)
);

SELECT create_hypertable('strategy_metrics', 'timestamp', if_not_exists => TRUE);

-- ============================================================================
-- REFERENCE DATA
-- ============================================================================

-- Instruments metadata
CREATE TABLE IF NOT EXISTS instruments (
    id VARCHAR(32) PRIMARY KEY,
    symbol VARCHAR(32) NOT NULL,
    asset_class VARCHAR(16) NOT NULL,  -- FX, METAL, INDEX
    base_currency VARCHAR(8),
    quote_currency VARCHAR(8),
    min_price_increment NUMERIC(20, 8),
    min_quantity NUMERIC(20, 4),
    contract_size NUMERIC(20, 4) DEFAULT 1,
    trading_hours VARCHAR(128),
    created_at TIMESTAMPTZ DEFAULT NOW()
);

-- Insert default instruments
INSERT INTO instruments (id, symbol, asset_class, base_currency, quote_currency, min_price_increment, min_quantity, contract_size)
VALUES
    ('EURUSD', 'EUR/USD', 'FX', 'EUR', 'USD', 0.00001, 1000, 100000),
    ('GBPUSD', 'GBP/USD', 'FX', 'GBP', 'USD', 0.00001, 1000, 100000),
    ('USDJPY', 'USD/JPY', 'FX', 'USD', 'JPY', 0.001, 1000, 100000),
    ('AUDUSD', 'AUD/USD', 'FX', 'AUD', 'USD', 0.00001, 1000, 100000),
    ('XAUUSD', 'Gold', 'METAL', 'XAU', 'USD', 0.01, 0.01, 100),
    ('XAGUSD', 'Silver', 'METAL', 'XAG', 'USD', 0.001, 1, 5000),
    ('DAX', 'DAX 40', 'INDEX', NULL, 'EUR', 0.5, 1, 25),
    ('ES', 'E-mini S&P 500', 'INDEX', NULL, 'USD', 0.25, 1, 50),
    ('NQ', 'E-mini NASDAQ-100', 'INDEX', NULL, 'USD', 0.25, 1, 20),
    ('US100', 'US Tech 100', 'INDEX', NULL, 'USD', 0.1, 1, 1)
ON CONFLICT (id) DO NOTHING;

-- ============================================================================
-- VIEWS
-- ============================================================================

-- Latest positions view
CREATE OR REPLACE VIEW latest_positions AS
SELECT DISTINCT ON (instrument)
    timestamp,
    instrument,
    quantity,
    average_price,
    unrealized_pnl,
    realized_pnl
FROM positions
ORDER BY instrument, timestamp DESC;

-- Active orders view
CREATE OR REPLACE VIEW active_orders AS
SELECT *
FROM orders
WHERE status IN ('PENDING', 'SUBMITTED', 'ACKNOWLEDGED', 'PARTIALLY_FILLED')
ORDER BY created_at DESC;

-- Daily P&L view
CREATE OR REPLACE VIEW daily_pnl AS
SELECT
    DATE(timestamp) as date,
    SUM(pnl) as total_pnl,
    COUNT(*) as trade_count
FROM trades
GROUP BY DATE(timestamp)
ORDER BY date DESC;

-- ============================================================================
-- RETENTION POLICIES
-- ============================================================================

-- Retain tick data for 7 days (can be adjusted)
-- SELECT add_retention_policy('ticks', INTERVAL '7 days');

-- Retain 1m bars for 90 days
-- SELECT add_retention_policy('bars_1m', INTERVAL '90 days');

-- Longer timeframe bars retained longer
-- SELECT add_retention_policy('bars_1d', INTERVAL '5 years');

-- ============================================================================
-- GRANTS
-- ============================================================================

-- TODO: Create application user and grant permissions
-- CREATE USER marketecho_app WITH PASSWORD 'secure_password';
-- GRANT SELECT, INSERT, UPDATE ON ALL TABLES IN SCHEMA public TO marketecho_app;
-- GRANT USAGE, SELECT ON ALL SEQUENCES IN SCHEMA public TO marketecho_app;
