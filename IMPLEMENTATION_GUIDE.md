# QuantumLiquidity Implementation Guide

This document outlines what needs to be implemented to complete the platform.

## Current Status

**Completed**:
- ✅ Full architecture design
- ✅ C++ project structure with CMake
- ✅ Python project structure
- ✅ PostgreSQL schema (TimescaleDB)
- ✅ Core interfaces and abstractions
- ✅ Configuration system
- ✅ Docker infrastructure

**In Progress** (Skeleton/Stub):
- ⚠️ C++ implementations (interfaces defined, logic needed)
- ⚠️ Python services (structure ready, logic needed)
- ⚠️ Strategy implementations
- ⚠️ Provider integrations

## Priority Implementation Order

### Phase 1: Foundation (Week 1-2)

#### 1.1 Database Layer
**Files**: `cpp/src/persistence/*`

- [ ] Implement PostgreSQL connection pool
- [ ] Batch writer for ticks/bars
- [ ] Reader for historical data queries
- [ ] Redis pub/sub client

**Libraries needed**: `libpq` (PostgreSQL), `hiredis` (Redis)

#### 1.2 Logging
**Files**: `cpp/src/common/logger.cpp`

- [ ] Integrate spdlog
- [ ] Configure separate channels (market_data, orders, risk, etc.)
- [ ] Add structured logging with context

**Library**: `spdlog`

#### 1.3 Configuration
**Files**: `cpp/src/common/config.cpp`

- [ ] YAML configuration loading
- [ ] Environment variable override
- [ ] Validation

**Library**: `yaml-cpp`

### Phase 2: Market Data (Week 2-3)

#### 2.1 Market Data Gateway
**Files**: `cpp/src/market_data/*`

- [ ] Feed manager orchestration
- [ ] Bar aggregator (ticks → bars)
- [ ] Redis publisher
- [ ] PostgreSQL writer integration
- [ ] Replay feed for backtesting

#### 2.2 Provider Implementation (Choose One First)
**Files**: `cpp/src/market_data/providers/*`

**Option A: CSV/File Provider** (Easiest, for testing)
- [ ] Read ticks/bars from CSV
- [ ] Normalize to internal format

**Option B: MT5 Bridge** (If using MetaTrader)
- [ ] MT5 Python API → Redis bridge
- [ ] C++ consumes from Redis

**Option C: REST/WebSocket Provider** (Darwinex, etc.)
- [ ] WebSocket connection handling
- [ ] JSON parsing
- [ ] Reconnection logic

**TODO**: Determine which broker/provider to implement first based on available accounts.

### Phase 3: Risk & Execution (Week 3-4)

#### 3.1 Position Manager
**Files**: `cpp/src/risk/position_manager.cpp`

- [ ] Track positions by instrument
- [ ] Calculate unrealized P&L (mark-to-market)
- [ ] Calculate realized P&L from fills
- [ ] Exposure calculation

#### 3.2 Risk Rules
**Files**: `cpp/src/risk/risk_rules.cpp`

Implement individual rules:
- [ ] `MaxPositionSizeRule`: Check position doesn't exceed limit
- [ ] `MaxExposureRule`: Check total account exposure
- [ ] `OrderRateLimitRule`: Check orders/minute threshold
- [ ] `MaxDailyLossRule`: Check daily P&L vs limit
- [ ] `LeverageRule`: Calculate and check leverage

#### 3.3 Risk Engine
**Files**: `cpp/src/risk/risk_engine.cpp`

- [ ] Complete pre-trade check logic
- [ ] Position update handling
- [ ] Kill-switch implementation
- [ ] Metrics calculation

#### 3.4 Execution Engine
**Files**: `cpp/src/execution/execution_engine.cpp`

- [ ] Order submission flow: strategy → risk → provider
- [ ] Order state tracking
- [ ] Fill processing
- [ ] Callback routing back to strategies

#### 3.5 Execution Provider
**Files**: `cpp/src/execution/providers/*`

**Start with**: Mock provider for testing
- [ ] Accept orders, immediately fill at current price
- [ ] Simulate slippage
- [ ] Random reject (5% of orders)

**Production**: Choose real provider
- [ ] FIX protocol (most professional)
- [ ] REST API (Darwinex, etc.)
- [ ] MT5 bridge

### Phase 4: Strategy Engine (Week 4-5)

#### 4.1 Strategy Framework
**Files**: `cpp/src/strategy/strategy_engine.cpp`, `strategy_context.cpp`

- [ ] Strategy registration
- [ ] Event routing (ticks/bars → strategies)
- [ ] Context implementation (order submission, queries, logging)
- [ ] Order update routing

#### 4.2 ORB Strategy
**Files**: `cpp/src/strategy/strategies/orb_strategy.cpp`

- [ ] Opening range detection
- [ ] Breakout detection
- [ ] Entry logic (immediate/retest/confirmation)
- [ ] Stop loss placement
- [ ] Target calculation based on R-multiple
- [ ] Filters: volatility, day type, sentiment

#### 4.3 Python Binding (Optional but Recommended)
**Files**: `cpp/bindings/python/*`

- [ ] pybind11 setup
- [ ] Expose C++ strategy interface to Python
- [ ] Example Python strategy

### Phase 5: Analytics (Week 5-6)

#### 5.1 Historical Data Service
**Files**: `python/quantumliquidity/historical_data/service.py`

- [ ] Provider API integrations (download historical data)
- [ ] CSV import utility
- [ ] Data validation and cleaning

#### 5.2 Day Classifier
**Files**: `python/quantumliquidity/analytics/day_classifier.py`

- [x] Algorithm implemented (needs testing)
- [ ] Batch processing for historical days
- [ ] Store results to `daily_stats` table
- [ ] Publish to Redis on classification

#### 5.3 ORB Analyzer
**Files**: `python/quantumliquidity/analytics/orb_analyzer.py`

- [ ] Complete statistics calculation
- [ ] R-multiple distribution
- [ ] Breakout probability by volatility regime
- [ ] Store to `orb_stats` table

#### 5.4 Volume Profile
**Files**: `python/quantumliquidity/analytics/volume_profile.py` (create)

- [ ] TPO chart construction
- [ ] VAH/VAL/POC calculation
- [ ] Profile shape classification

#### 5.5 Session Analyzer
**Files**: `python/quantumliquidity/analytics/session_analyzer.py` (create)

- [ ] Session range statistics
- [ ] Trend direction by session
- [ ] Session overlap analysis

### Phase 6: Sentiment (Week 6-7)

#### 6.1 News Parser
**Files**: `python/quantumliquidity/sentiment/news_parser.py`

- [ ] RSS feed fetching (forexfactory, investing.com)
- [ ] Economic calendar scraping
- [ ] Deduplication logic

#### 6.2 Instrument Mapper
**Files**: `python/quantumliquidity/sentiment/instrument_mapper.py` (create)

- [ ] Rule-based mapping (USD → EURUSD, GBPUSD, etc.)
- [ ] Keyword extraction
- [ ] Confidence scoring

#### 6.3 Sentiment Analyzer
**Files**: `python/quantumliquidity/sentiment/sentiment_analyzer.py`

- [ ] LLM integration (OpenAI/Anthropic)
- [ ] Prompt engineering for financial sentiment
- [ ] Batch processing for efficiency
- [ ] Caching to avoid duplicate analysis

#### 6.4 Aggregator
**Files**: `python/quantumliquidity/sentiment/aggregator.py` (create)

- [ ] Time-series aggregation (rolling window)
- [ ] Weighted by confidence and recency
- [ ] Publish to Redis on spikes/changes

### Phase 7: Monitoring API (Week 7)

#### 7.1 FastAPI Setup
**Files**: `python/quantumliquidity/api/main.py` (create)

- [ ] App initialization
- [ ] CORS setup
- [ ] Authentication (JWT)

#### 7.2 Endpoints
**Files**: `python/quantumliquidity/api/routes/*` (create)

- [ ] `/strategies`: List, start, stop
- [ ] `/positions`: Current positions
- [ ] `/orders`: Order history and active orders
- [ ] `/pnl`: P&L reports
- [ ] `/analytics/{instrument}`: Analytics data
- [ ] `/sentiment/{instrument}`: Sentiment time series

#### 7.3 WebSocket
**Files**: `python/quantumliquidity/api/websocket.py` (create)

- [ ] Subscribe to Redis channels
- [ ] Forward order updates to connected clients
- [ ] Forward position updates

### Phase 8: Testing (Week 8)

#### 8.1 Unit Tests

**C++**: `cpp/tests/*`
- [ ] Risk rules tests
- [ ] Position manager tests
- [ ] Bar aggregator tests
- [ ] Mock provider tests

**Python**: `python/tests/*`
- [ ] Day classifier tests (with sample data)
- [ ] ORB analyzer tests
- [ ] Sentiment analyzer tests (with mock LLM)

#### 8.2 Integration Tests

- [ ] End-to-end: CSV data → market data gateway → strategy → mock execution
- [ ] Database round-trip (write bars, read bars)
- [ ] Risk limit enforcement (submit order exceeding limit)

#### 8.3 Backtest

- [ ] Load historical data for DAX (Jan-Dec 2024)
- [ ] Run ORB strategy in replay mode
- [ ] Generate performance report
- [ ] Compare against buy-and-hold

## Third-Party Libraries to Install

### C++ Dependencies

Install via `vcpkg`, `conan`, or system package manager:

```bash
# Ubuntu/Debian
sudo apt-get install \
    libpq-dev \
    libhiredis-dev \
    libprotobuf-dev \
    libspdlog-dev \
    libyaml-cpp-dev \
    nlohmann-json3-dev \
    libgtest-dev \
    libbenchmark-dev

# macOS (Homebrew)
brew install \
    postgresql \
    hiredis \
    protobuf \
    spdlog \
    yaml-cpp \
    nlohmann-json \
    googletest \
    google-benchmark
```

Or use vcpkg:
```bash
vcpkg install \
    libpq \
    hiredis \
    protobuf \
    spdlog \
    yaml-cpp \
    nlohmann-json \
    gtest \
    benchmark
```

### Python Dependencies

Already specified in `pyproject.toml`. Install with:
```bash
cd python
pip install -e ".[dev,ai]"
```

## Provider-Specific TODOs

### Darwinex
- [ ] Register for API access
- [ ] Obtain API key/secret
- [ ] Implement FIX or REST connector

### IC Markets (MT5)
- [ ] Open demo account
- [ ] Install MetaTrader 5
- [ ] Implement Python MT5 bridge script
- [ ] Test tick streaming

### Interactive Brokers
- [ ] Open account (paper trading available)
- [ ] Install TWS or IB Gateway
- [ ] Use IB Python API or FIX
- [ ] Handle tick throttling (snapshot vs streaming)

## Development Workflow

1. **Pick a Phase**: Start with Phase 1 (Foundation)
2. **Implement**: Write code for one component at a time
3. **Test**: Write unit test before moving on
4. **Integrate**: Connect to rest of system
5. **Document**: Add comments and update docs
6. **Commit**: Small, atomic commits with clear messages

## Code Quality Standards

- **C++**: Modern C++20, RAII, no raw pointers, clear ownership
- **Python**: Type hints everywhere, pydantic for validation
- **Comments**: Explain "why", not "what"
- **Tests**: >80% coverage target
- **Performance**: Profile before optimizing (but design for performance)

## Questions to Resolve

- [ ] Which broker(s) will be used initially?
- [ ] Which LLM provider for sentiment (OpenAI vs Anthropic)?
- [ ] Paper trading duration before going live?
- [ ] Position sizing: fixed contracts vs risk-based?
- [ ] Commission/slippage assumptions for backtests?
- [ ] Deployment target: local server, cloud, VPS?

## Next Steps

1. Review this guide
2. Choose initial broker/provider
3. Start Phase 1: Database + Logging + Config
4. Set up daily standup/review cadence
5. Track progress in project management tool

---

**Estimated Total Time**: 8-10 weeks for MVP with 1 developer working full-time.

**Can be parallelized**: Front-load C++ core, overlap with Python analytics development.
