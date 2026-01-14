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
- **AI Sentiment Engine**: LLM-powered news analysis and sentiment aggregation
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

**Phase 1: Foundation (COMPLETE)** ✅  
PostgreSQL pool, Redis pub/sub, Logging, Configuration

**Overall Progress**: ~20% (Phase 1 of 8)

See [PROGRESS.md](PROGRESS.md) for details.

## Documentation

- [Architecture](ARCHITECTURE.md)
- [Implementation Guide](IMPLEMENTATION_GUIDE.md)
- [Project Structure](PROJECT_STRUCTURE.md)

## License

Proprietary - All rights reserved.
