# OANDA Live Data Setup Guide

This guide explains how to connect to OANDA's live market data streaming API.

## Prerequisites

1. **OANDA Account** (Practice or Live)
   - Create a practice (demo) account: https://www.oanda.com/register/
   - Or use your existing OANDA live account

2. **API Token**
   - Log in to OANDA
   - Go to: https://www.oanda.com/account/tpa/personal_token
   - Generate a new Personal Access Token
   - **Save it securely** - you won't be able to see it again

3. **Account ID**
   - Find it in your OANDA dashboard
   - Format: `XXX-XXX-XXXXXXXX-XXX`

## Quick Start

### 1. Get OANDA Credentials

```bash
# Sign up for practice account
open https://www.oanda.com/register/

# Get your API token
open https://www.oanda.com/account/tpa/personal_token
```

### 2. Set Environment Variables

```bash
export OANDA_API_TOKEN="your-api-token-here"
export OANDA_ACCOUNT_ID="XXX-XXX-XXXXXXXX-XXX"
```

### 3. Run Example

```bash
cd cpp/build
./examples/oanda_live_example
```

## Usage Example

```cpp
#include "quantumliquidity/market_data/oanda_feed.hpp"

// Configure
OANDAFeed::Config config;
config.api_token = getenv("OANDA_API_TOKEN");
config.account_id = getenv("OANDA_ACCOUNT_ID");
config.use_practice = true;  // Demo account

// Create and connect
auto feed = std::make_shared<OANDAFeed>(config);
feed_manager->add_feed(feed);
feed_manager->subscribe_instrument("EUR/USD");
feed_manager->start();
```

## Supported Instruments

- **Major Pairs**: EUR/USD, GBP/USD, USD/JPY, AUD/USD, USD/CHF, USD/CAD, NZD/USD
- **Minor Pairs**: EUR/GBP, EUR/JPY, GBP/JPY, AUD/JPY, EUR/AUD
- **Cross Rates**: EUR/CHF, GBP/CHF, AUD/NZD, etc.

## Documentation

Full documentation: [OANDA_SETUP.md](docs/OANDA_SETUP.md)

- API endpoints (practice vs live)
- Rate limits and best practices
- Troubleshooting guide
- Security recommendations
