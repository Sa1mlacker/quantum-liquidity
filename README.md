# QuantumLiquidity

Professional-grade HFT/Quant trading platform for FX, metals, and indices.

[![License](https://img.shields.io/badge/license-Proprietary-red.svg)](LICENSE)
[![C++](https://img.shields.io/badge/C++-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![Python](https://img.shields.io/badge/Python-3.11+-blue.svg)](https://www.python.org/)

## Overview

QuantumLiquidity is an institutional-grade algorithmic trading platform designed for professional prop firms and hedge funds. Built with a focus on low-latency execution, risk management, and advanced analytics.

### Key Features

- **Event-Driven Architecture**: Microsecond-latency market data processing
- **Multi-Asset Support**: FX pairs, precious metals, equity indices
- **Advanced Analytics**: Day type classification, ORB statistics, volume profile analysis
- **AI Sentiment Engine**: News analysis and sentiment aggregation
- **Professional Risk Management**: Pre-trade checks, position limits, kill-switch
- **Backtesting**: Historical market replay with configurable speed

## Quick Start

### Prerequisites

- Docker & Docker Compose
- C++20 compiler (GCC 11+, Clang 14+)
- CMake 3.20+
- Python 3.11+

### Installation

```bash
# 1. Start infrastructure
docker-compose up -d

# 2. Build C++ core
cd cpp && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# 3. Install Python
cd ../../python && pip install -e ".[dev]"

# 4. Configure
cp config/example.yaml config/local.yaml
cp .env.example .env
```

## Current Status

**Phase 1: Foundation** - COMPLETE
PostgreSQL pool, Redis pub/sub, Logging, Configuration

**Phase 2: Market Data Gateway** - COMPLETE
Bar aggregation, Feed manager, CSV replay, OANDA live streaming

**Phase 3: Execution & Risk** - COMPLETE
Position Manager, Risk Manager, Execution Engine, Mock Broker, 25+ tests

**Phase 4: REST API Layer** - COMPLETE
FastAPI with MessagePack, Redis caching, WebSocket streaming, 25+ endpoints

**Phase 5: Desktop App** - COMPLETE
Tauri + React + TypeScript, TradingView Lightweight Charts, 3 pages (Dashboard, Positions, Risk), Virtual scrolling

**Phase 6: Analytics & Intelligence** - COMPLETE
Day classification (TREND/RANGE/V_DAY/P_DAY), ORB analysis, Win rate & profit factor, REST API integration

**Phase 7: Strategy Framework** - COMPLETE
Base strategy class, ORB breakout strategy, Strategy manager, REST API control, Performance tracking

**Phase 8: Sentiment Engine** - COMPLETE
News analysis, Multi-source scraping, Real-time sentiment scoring, Alert system

**Overall Progress**: 100% COMPLETE (All 8 Phases)

See [docs/](docs/) for phase details.

## Live Market Data

Stream real-time FX data from OANDA:

```bash
# 1. Get OANDA demo account
open https://www.oanda.com/register/

# 2. Set credentials
export OANDA_API_TOKEN="your-token"
export OANDA_ACCOUNT_ID="your-account-id"

# 3. Run
cd cpp/build
./examples/oanda_live_example
```

Full guide: [docs/QUICK_START_LIVE_DATA.md](docs/QUICK_START_LIVE_DATA.md)

## Documentation

- [Architecture](ARCHITECTURE.md)
- [Implementation Guide](IMPLEMENTATION_GUIDE.md)
- [Project Structure](PROJECT_STRUCTURE.md)

## License

Proprietary - All rights reserved.
