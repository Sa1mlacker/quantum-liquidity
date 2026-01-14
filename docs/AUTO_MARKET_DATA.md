# Automatic Market Data - Zero Code Setup

Stream live data from multiple sources by just editing a YAML file. No code required!

## Quick Start (2 minutes)

### 1. Edit Config File

```bash
# Edit config/market_data.yaml
vim config/market_data.yaml
```

Add your instruments:

```yaml
feeds:
  - name: oanda_fx
    type: oanda
    enabled: true
    credentials:
      api_token: ${OANDA_API_TOKEN}
      account_id: ${OANDA_ACCOUNT_ID}
      use_practice: true
    
    instruments:
      - EUR/USD
      - GBP/USD
      - USD/JPY
      # Add more here!
```

### 2. Set Credentials

```bash
export OANDA_API_TOKEN="your-token"
export OANDA_ACCOUNT_ID="your-account-id"
```

### 3. Run

```bash
cd cpp/build
./market_data_daemon ../config/market_data.yaml
```

**That's it!** All instruments are automatically:
- ✅ Subscribed to live feed
- ✅ Aggregated into bars (1m, 5m, 15m, 1h, etc.)
- ✅ Saved to PostgreSQL
- ✅ Published to Redis

## What You See

```
=== QuantumLiquidity Market Data Daemon ===
Automatic feed configuration from YAML

Loading configuration: config/market_data.yaml
Connecting to database: quantumliquidity@localhost
Connecting to Redis: localhost

=== Configuring Feeds ===

Configuring feed: oanda_fx (oanda)
✓ OANDA feed configured with 13 instruments

=== Summary ===
Total instruments subscribed: 13
Timeframes: 7
Database persistence: enabled
Redis publishing: enabled

Starting feed manager...

=== Streaming Started ===
All feeds are now live!
Press Ctrl+C to stop

=== Stats (last 30s) ===
Ticks: 423 received, 423 written
Bars: 15 completed, 15 written
Redis: 438 publishes
Errors: 0
```

## Configuration Format

### Basic Feed

```yaml
feeds:
  - name: my_feed_name
    type: oanda  # or polygon, alphavantage
    enabled: true
    
    credentials:
      api_token: ${ENV_VAR}  # From environment
      account_id: ${ENV_VAR}
    
    instruments:
      - EUR/USD
      - GBP/USD
```

### Multiple Feeds

```yaml
feeds:
  # Feed 1: OANDA FX
  - name: oanda_fx
    type: oanda
    enabled: true
    instruments:
      - EUR/USD
      - GBP/USD
  
  # Feed 2: Polygon Stocks
  - name: polygon_stocks
    type: polygon
    enabled: true
    credentials:
      api_key: ${POLYGON_API_KEY}
    instruments:
      - AAPL
      - GOOGL
      - TSLA
```

### Timeframes

```yaml
aggregation:
  enabled: true
  timeframes:
    - 1m   # 1 minute
    - 5m   # 5 minutes
    - 15m  # 15 minutes
    - 1h   # 1 hour
    - 4h   # 4 hours
    - 1d   # 1 day
```

### Persistence

```yaml
persistence:
  database:
    enabled: true
    batch_size: 1000
    flush_interval_ms: 1000
  
  redis:
    enabled: true
    channels:
      ticks: market.ticks
      bars: market.bars
```

## Environment Variables

Set these before running:

```bash
# OANDA
export OANDA_API_TOKEN="your-token"
export OANDA_ACCOUNT_ID="your-account-id"

# Database (optional, defaults to localhost)
export DATABASE_HOST="localhost"
export DATABASE_PORT="5432"
export DATABASE_NAME="quantumliquidity"
export DATABASE_USER="quantumliquidity"
export DATABASE_PASSWORD=""

# Redis (optional, defaults to localhost)
export REDIS_HOST="localhost"
export REDIS_PORT="6379"

# Polygon.io (if using)
export POLYGON_API_KEY="your-key"

# Alpha Vantage (if using)
export ALPHAVANTAGE_API_KEY="your-key"
```

## Add New Instruments

Just edit the YAML and restart:

```yaml
instruments:
  - EUR/USD
  - GBP/USD
  - USD/JPY
  - AUD/USD  # Add this
  - USD/CHF  # Add this
```

```bash
# Ctrl+C to stop, then restart
./market_data_daemon ../config/market_data.yaml
```

## Add New Feed

```yaml
feeds:
  # Existing OANDA feed
  - name: oanda_fx
    type: oanda
    enabled: true
    instruments: [...]
  
  # Add Polygon.io
  - name: polygon_stocks
    type: polygon
    enabled: true
    credentials:
      api_key: ${POLYGON_API_KEY}
    instruments:
      - AAPL
      - GOOGL
      - MSFT
```

## Disable/Enable Feeds

```yaml
feeds:
  - name: oanda_fx
    type: oanda
    enabled: false  # Temporarily disable
```

## Production Deployment

### As systemd service

```bash
# Create service file
sudo vim /etc/systemd/system/quantumliquidity-market-data.service
```

```ini
[Unit]
Description=QuantumLiquidity Market Data Daemon
After=network.target postgresql.service redis.service

[Service]
Type=simple
User=trading
WorkingDirectory=/opt/quantumliquidity
Environment="OANDA_API_TOKEN=your-token"
Environment="OANDA_ACCOUNT_ID=your-account-id"
ExecStart=/opt/quantumliquidity/bin/market_data_daemon /opt/quantumliquidity/config/market_data.yaml
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
```

```bash
# Enable and start
sudo systemctl enable quantumliquidity-market-data
sudo systemctl start quantumliquidity-market-data

# Check status
sudo systemctl status quantumliquidity-market-data

# View logs
sudo journalctl -u quantumliquidity-market-data -f
```

### With Docker

```dockerfile
FROM ubuntu:22.04
RUN apt-get update && apt-get install -y libcurl4 libyaml-cpp-dev
COPY market_data_daemon /usr/local/bin/
COPY config/market_data.yaml /etc/quantumliquidity/
ENV OANDA_API_TOKEN=${OANDA_API_TOKEN}
ENV OANDA_ACCOUNT_ID=${OANDA_ACCOUNT_ID}
CMD ["market_data_daemon", "/etc/quantumliquidity/market_data.yaml"]
```

## Monitoring

### Check Redis

```bash
redis-cli SUBSCRIBE market.ticks
redis-cli SUBSCRIBE market.bars
```

### Check PostgreSQL

```sql
-- Latest ticks
SELECT instrument, timestamp, bid, ask 
FROM ticks 
ORDER BY timestamp DESC 
LIMIT 10;

-- Latest 1m bars
SELECT instrument, timestamp, open, high, low, close, volume
FROM bars_1m
ORDER BY timestamp DESC
LIMIT 10;
```

### Health Check Endpoint

TODO: Add HTTP health check endpoint to daemon

## Troubleshooting

**"Config file not found"**
```bash
# Make sure path is correct
ls -l config/market_data.yaml

# Or specify absolute path
./market_data_daemon /full/path/to/config/market_data.yaml
```

**"OANDA credentials not found"**
```bash
# Check environment variables
echo $OANDA_API_TOKEN
echo $OANDA_ACCOUNT_ID

# Make sure they're exported
export OANDA_API_TOKEN="..."
```

**"Failed to parse YAML"**
```bash
# Check YAML syntax
python3 -c "import yaml; yaml.safe_load(open('config/market_data.yaml'))"
```

**No data received**
- Check market hours (FX trades 24/5)
- Verify credentials are correct
- Check OANDA status: https://status.oanda.com/
- Look for errors in logs

## Benefits

✅ **Zero code** - just edit YAML
✅ **Multiple feeds** - OANDA, Polygon, Alpha Vantage, etc.
✅ **Auto-reconnect** - handles disconnections
✅ **Auto-aggregation** - ticks → bars automatically
✅ **Auto-persistence** - PostgreSQL + Redis
✅ **Production-ready** - systemd service, Docker support
✅ **Hot-reload** - restart to apply config changes

## Next Steps

- Add more instruments to YAML
- Add more feeds (Polygon, Alpha Vantage)
- Monitor data in PostgreSQL/Redis
- Build strategies using live data
- Deploy to production with systemd/Docker
