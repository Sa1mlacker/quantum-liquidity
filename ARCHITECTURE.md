# QuantumLiquidity Architecture

## Overview
Professional-grade HFT/Quant trading platform for FX, metals, and indices.

## Technology Stack

### Core Components
- **C++20**: Execution engine, market data processing, risk management
- **Python 3.11**: Analytics, AI/ML, research, orchestration
- **PostgreSQL**: Time-series data, trades, analytics results
- **Redis**: Event bus, real-time messaging, cache
- **Protocol Buffers**: Inter-service communication

### Infrastructure
- Event-driven architecture
- Market replay capability for backtesting
- Centralized risk management
- Multi-broker abstraction layer

## Module Architecture

### 1. Market Data Gateway (C++)
**Location**: `cpp/src/market_data/`

**Responsibilities**:
- Connect to broker APIs (FIX, WebSocket, REST)
- Subscribe to ticks, bars, depth data
- Normalize and timestamp data
- Publish to Redis event bus
- Store raw data to PostgreSQL
- Support record/replay mode

**Key Interfaces**:
- `IMarketDataFeed`: Abstract feed interface
- `IMarketDataProvider`: Broker-specific implementations
- Events: `on_tick`, `on_bar`, `on_depth_update`

**Supported Instruments**:
- FX: EURUSD, GBPUSD, USDJPY, AUDUSD, USDCHF, EURGBP
- Metals: XAUUSD (gold), XAGUSD (silver)
- Indices: DAX, ES, NQ, US100 CFD

### 2. Historical Data Service (Python)
**Location**: `python/quantumliquidity/historical_data/`

**Responsibilities**:
- Download historical data from brokers/vendors
- Normalize tick and bar data
- Store in PostgreSQL with proper indexing
- Provide query API for research and backtests

**API**:
```python
class HistoricalDataService:
    async def download_bars(instrument, start_date, end_date, timeframe)
    async def get_bars(instrument, start_date, end_date, timeframe)
    async def get_ticks(instrument, start_date, end_date)
```

### 3. Analytics Engine (Python)
**Location**: `python/quantumliquidity/analytics/`

**Responsibilities**:
- Day type classification (trend, range, V-day, P-day)
- ORB (Opening Range Breakout) statistics
- Volatility metrics (ATR, realized vol)
- Volume Profile (VAH, VAL, POC)
- Session analysis (Asia, London, NY)

**Modules**:
- `day_classifier.py`: Day type detection
- `orb_analyzer.py`: ORB statistics and probabilities
- `volatility.py`: ATR, realized vol, GARCH
- `volume_profile.py`: TPO/Volume Profile construction
- `session_analyzer.py`: Session-specific metrics

**Output**:
- Tables: `daily_stats`, `session_stats`, `orb_stats`
- Redis: `analytics:signals` channel

### 4. AI Sentiment Engine (Python)
**Location**: `python/quantumliquidity/sentiment/`

**Responsibilities**:
- Parse news feeds (RSS, APIs)
- Map news to instruments (USD → FX pairs, XAU → gold)
- LLM-based sentiment analysis (bullish/bearish/neutral)
- Event classification (macro, monetary policy, geopolitical)
- Aggregate sentiment time series

**Modules**:
- `news_parser.py`: RSS/API fetching
- `instrument_mapper.py`: News → instrument mapping
- `sentiment_analyzer.py`: LLM interface
- `aggregator.py`: Time-series aggregation

**Output**:
- Tables: `news_events`, `sentiment_series`
- Redis: `sentiment:signals` channel

### 5. Strategy Engine (C++ + Python)
**Location**: `cpp/src/strategy/` + `python/quantumliquidity/strategies/`

**Responsibilities**:
- Event-driven strategy framework
- Receive market data, analytics, sentiment events
- Generate order requests
- Support both C++ and Python strategies (via pybind11)

**Core Strategies**:
1. **ORB Breakout** (indices): Trade opening range breaks with vol/sentiment filters
2. **Mean Reversion** (metals): Fade extremes on range days
3. **Sentiment Filter**: Block trades against strong sentiment moves

**Interfaces**:
```cpp
class IStrategy {
    virtual void on_tick(const Tick&) = 0;
    virtual void on_bar(const Bar&) = 0;
    virtual void on_event(const AnalyticsEvent&) = 0;
    virtual void on_order_update(const OrderUpdate&) = 0;
    virtual std::string name() const = 0;
};
```

### 6. Execution & Risk Engine (C++)
**Location**: `cpp/src/execution/`, `cpp/src/risk/`

**Responsibilities**:
- Receive order requests from strategies
- Pre-trade risk checks:
  - Max position per instrument
  - Max account exposure
  - Order rate limits
  - Max daily loss (kill-switch)
- Route orders to brokers
- Process fills and update positions
- Comprehensive logging

**Interfaces**:
```cpp
class IOrderSender {
    virtual OrderID submit_order(const OrderRequest&) = 0;
    virtual void cancel_order(OrderID) = 0;
    virtual void modify_order(OrderID, const OrderModification&) = 0;
};

class IRiskEngine {
    virtual RiskCheckResult check_order(const OrderRequest&) = 0;
    virtual void update_position(const Fill&) = 0;
    virtual RiskMetrics get_metrics() const = 0;
};
```

**Risk Rules**:
- Per-instrument position limits (configurable)
- Account-level exposure limit (% of NAV)
- Max orders per minute (anti-fat-finger)
- Max daily loss (hard stop, requires manual reset)

### 7. Monitoring API (Python FastAPI)
**Location**: `python/quantumliquidity/api/`

**Responsibilities**:
- REST API for system monitoring
- WebSocket feeds for real-time updates
- Strategy management (start/stop/configure)
- Position and PnL views
- Analytics and sentiment dashboards

**Endpoints**:
- `GET /strategies`: List all strategies and their states
- `POST /strategies/{id}/start`: Start strategy
- `POST /strategies/{id}/stop`: Stop strategy
- `GET /positions`: Current positions
- `GET /pnl`: PnL breakdown by strategy/instrument
- `GET /analytics/{instrument}`: Latest analytics for instrument
- `GET /sentiment/{instrument}`: Sentiment time series
- `WS /stream/orders`: Real-time order updates
- `WS /stream/market`: Real-time market data

## Data Model

### PostgreSQL Schema

**Market Data**:
- `ticks`: Raw tick data (timestamp, instrument, bid, ask, volume)
- `bars_1m`, `bars_5m`, `bars_1h`, `bars_1d`: OHLCV bars by timeframe

**Analytics**:
- `daily_stats`: Day type, ranges, volatility
- `session_stats`: Asia/London/NY session metrics
- `orb_stats`: Opening range statistics and probabilities
- `volume_profile`: VAH/VAL/POC by day

**Sentiment**:
- `news_events`: Raw news items
- `sentiment_series`: Aggregated sentiment scores

**Trading**:
- `orders`: All order requests and states
- `trades`: Executed trades (fills)
- `positions`: Position snapshots

**Strategy Performance**:
- `strategy_metrics`: PnL, Sharpe, drawdown by strategy

### Redis Channels

- `market:ticks:{instrument}`: Tick stream
- `market:bars:{instrument}:{tf}`: Bar stream
- `analytics:signals`: Analytics events (ORB break, day type change)
- `sentiment:signals`: Sentiment spikes/changes
- `orders:updates`: Order state changes
- `risk:alerts`: Risk limit breaches

## Event Flow

### Market Data Path
```
Broker → MarketDataGateway → Redis Pub → [Strategies, Analytics]
                          ↓
                     PostgreSQL (raw storage)
```

### Order Execution Path
```
Strategy → OrderRequest → RiskEngine → ExecutionProvider → Broker
                              ↓             ↓
                         RiskMetrics    OrderUpdate
                              ↓             ↓
                         PostgreSQL    Redis Pub → Strategy
```

### Analytics Path
```
PostgreSQL (historical data) → AnalyticsEngine → Results
                                                     ↓
                                              PostgreSQL + Redis
```

## Configuration

Configuration files use YAML format:

- `config/market_data.yaml`: Feed connections, instruments
- `config/risk.yaml`: Position limits, loss limits, rate limits
- `config/strategies.yaml`: Strategy parameters
- `config/database.yaml`: PostgreSQL/Redis connection strings

## Logging

Structured logging with separate channels:

- `logs/market_data.log`: Tick/bar reception
- `logs/orders.log`: Order lifecycle (submit/fill/cancel)
- `logs/risk.log`: Risk checks and breaches
- `logs/strategies.log`: Strategy signals and state changes
- `logs/errors.log`: System errors
- `logs/performance.log`: Latency metrics

## Deployment

### Development
```bash
# Start infrastructure
docker-compose up -d postgres redis

# Build C++ core
cd cpp && mkdir build && cd build
cmake .. && make -j

# Install Python packages
cd python && pip install -e .

# Run services
./scripts/start_market_data.sh
./scripts/start_analytics.sh
./scripts/start_strategies.sh
./scripts/start_api.sh
```

### Production
- Containerized deployment (Docker/Kubernetes)
- Separate containers for each major component
- Shared PostgreSQL/Redis infrastructure
- Health checks and auto-restart
- Prometheus metrics export

## Testing

### Unit Tests
- C++: Google Test framework
- Python: pytest

### Integration Tests
- End-to-end market data flow
- Order execution with mock broker
- Risk limit enforcement

### Backtests
- Historical replay mode
- Strategy performance validation
- Risk metrics verification

## Performance Targets

- **Market Data Latency**: < 1ms tick-to-strategy
- **Order Submission**: < 500μs strategy-to-wire
- **Risk Check**: < 100μs per order
- **Throughput**: 10k+ ticks/sec per instrument
- **Memory**: Bounded, no leaks in 24/7 operation

## Security

- API authentication (JWT tokens)
- Broker credentials in secure vault
- Audit logging of all order activity
- Network isolation for trading components
- Rate limiting on external APIs

## Future Enhancements

- [ ] FIX protocol support for tier-1 brokers
- [ ] Multi-leg orders (spreads, combos)
- [ ] Machine learning strategy framework
- [ ] Real-time portfolio optimization
- [ ] Advanced order types (iceberg, TWAP, VWAP)
- [ ] Multi-account support
- [ ] Regulatory reporting (MiFID II, etc.)
