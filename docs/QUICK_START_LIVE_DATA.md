# Quick Start: Live Market Data

Get real-time FX data streaming in 5 minutes!

## Option 1: OANDA (Recommended for FX)

### Step 1: Create Account (2 min)
1. Go to https://www.oanda.com/register/
2. Sign up for **practice (demo) account** - it's free!
3. Verify your email

### Step 2: Get API Credentials (1 min)
1. Login to OANDA
2. Go to: https://www.oanda.com/account/tpa/personal_token
3. Click "Generate" to create Personal Access Token
4. **Copy the token immediately** (you can't see it again!)
5. Note your Account ID from dashboard (format: XXX-XXX-XXXXXXXX-XXX)

### Step 3: Configure (30 sec)
```bash
# Add to your shell config (~/.bashrc or ~/.zshrc)
export OANDA_API_TOKEN="your-token-here"
export OANDA_ACCOUNT_ID="XXX-XXX-XXXXXXXX-XXX"

# Or just for current session
export OANDA_API_TOKEN="paste-your-token"
export OANDA_ACCOUNT_ID="paste-your-account-id"
```

### Step 4: Build & Run (1 min)
```bash
# Install libcurl if needed
brew install curl  # macOS
# or: sudo apt-get install libcurl4-openssl-dev  # Ubuntu

# Build
cd cpp
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Run
./examples/oanda_live_example
```

### What You'll See

```
[INFO] OANDA feed initialized (mode: practice)
[INFO] OANDA account verified: XXX-XXX-XXXXXXXX-XXX
[INFO] OANDA feed connected
[INFO] Subscribing to instruments:
[INFO]   - EUR/USD
[INFO]   - GBP/USD
[INFO]   - USD/JPY
[INFO] OANDA streaming thread started
[INFO] === Live streaming started ===
[INFO] Press Ctrl+C to stop

[INFO] === Statistics (last 10s) ===
[INFO] Ticks received: 147
[INFO] Ticks written: 147
[INFO] Bars completed: 5
[INFO] Bars written: 5
```

**You're now streaming live FX data!** 🎉

Data flows:
- ✅ Ticks → PostgreSQL (for historical storage)
- ✅ Ticks → Redis (for real-time distribution)
- ✅ Bars (1m, 5m, 15m, 1h) → PostgreSQL
- ✅ Bars → Redis

## Option 2: CSV Replay (Testing Without Broker)

```bash
cd cpp/build
./examples/market_data_example ../data/sample_ticks.csv
```

Uses sample data for testing. Good for development, not for live trading.

## Next Steps

### 1. Monitor Live Data

**Redis subscriber** (in another terminal):
```bash
redis-cli
SUBSCRIBE market.ticks
SUBSCRIBE market.bars
```

You'll see JSON messages like:
```json
{"timestamp":1704182400000,"instrument":"EUR/USD","bid":1.10245,"ask":1.10250}
```

**PostgreSQL query**:
```sql
-- Latest ticks
SELECT * FROM ticks ORDER BY timestamp DESC LIMIT 10;

-- Latest 1-minute bars
SELECT * FROM bars_1m ORDER BY timestamp DESC LIMIT 10;
```

### 2. Add More Instruments

Edit `oanda_live_example.cpp`:
```cpp
std::vector<std::string> instruments = {
    "EUR/USD",
    "GBP/USD",
    "USD/JPY",
    "AUD/USD",
    "USD/CHF",
    "EUR/GBP",  // Add more here
    "GBP/JPY"
};
```

Rebuild and run.

### 3. Subscribe to Different Timeframes

Edit feed manager config:
```cpp
fm_config.default_timeframes = {
    TimeFrame::MIN_1,    // 1 minute
    TimeFrame::MIN_5,    // 5 minutes
    TimeFrame::MIN_15,   // 15 minutes
    TimeFrame::MIN_30,   // 30 minutes
    TimeFrame::HOUR_1,   // 1 hour
    TimeFrame::HOUR_4,   // 4 hours
    TimeFrame::DAY_1     // 1 day
};
```

### 4. Build Your Strategy

Now that you have live data, you can:
- Analyze tick-by-tick price movements
- Calculate indicators on bars
- Test strategies with live data
- Move to Phase 3: Risk & Execution Engine

## Troubleshooting

**"OANDA credentials not found"**
- Check environment variables: `echo $OANDA_API_TOKEN`
- Make sure you exported them in current shell

**"Authorization failed"**
- Verify API token is correct (copy-paste carefully)
- Token might have expired (generate new one)

**"Account not found"**
- Check account ID format: XXX-XXX-XXXXXXXX-XXX
- Ensure using practice endpoints (use_practice = true)

**No data received**
- Check market hours (FX trades 24/5, closed weekends)
- Verify instruments are subscribed
- Check OANDA status: https://status.oanda.com/

**Build errors**
- Install libcurl: `brew install curl` or `apt-get install libcurl4-openssl-dev`
- Install nlohmann-json: `brew install nlohmann-json` or include in project

## Full Documentation

- [OANDA Setup Guide](docs/OANDA_SETUP.md) - Complete OANDA documentation
- [Phase 2 Documentation](PHASE2_COMPLETE.md) - Market Data Gateway details
- [Architecture](ARCHITECTURE.md) - System architecture overview

## Support

Having issues? Check:
1. [Troubleshooting section](#troubleshooting) above
2. [OANDA API Docs](https://developer.oanda.com/rest-live-v20/)
3. GitHub Issues: https://github.com/Sa1mlacker/quantum-liquidity/issues

Happy trading! 📈
