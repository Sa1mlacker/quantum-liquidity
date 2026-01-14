# QuantumLiquidity Desktop App

**Status:** In Progress (Phase 5)

Professional trading desktop application built with Tauri + React.

## Features (Planned)

- ✅ **Tauri Framework** - 15-20 MB bundle (vs 150-200 MB Electron)
- ✅ **React + TypeScript** - Type-safe UI development
- ✅ **TradingView Lightweight Charts** - High-performance charting
- ✅ **MessagePack WebSocket** - 10x smaller messages than JSON
- ✅ **React Window** - Virtual scrolling for large datasets
- 🚧 **Real-time Dashboard** - Live positions, PnL, charts
- 🚧 **Risk Monitor** - Real-time risk metrics and alerts
- 🚧 **Trade History** - Paginated table with filters

## Architecture

```
Desktop App (Tauri + React)
    ↓ HTTP/WebSocket
FastAPI Backend
    ↓
C++ QuantumLiquidity Core
```

## Performance Targets

- Bundle size: ≤20 MB
- Idle RAM: ≤50 MB
- Idle CPU: <0.1%
- Chart render: <50ms
- Table scroll: 60 FPS
- Startup: <300ms

## Development

### Prerequisites

```bash
# Node.js 18+
node --version

# Rust (for Tauri)
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
```

### Install Dependencies

```bash
cd desktop
npm install
```

### Run Development Server

```bash
# Terminal 1: Start FastAPI backend
cd ../python
uvicorn quantumliquidity.api.main:app --reload

# Terminal 2: Start Tauri dev
cd ../desktop
npm run tauri:dev
```

App opens at `http://localhost:1420` (Vite dev server) and Tauri window.

### Build Production

```bash
npm run tauri:build
```

Output:
- macOS: `src-tauri/target/release/bundle/dmg/QuantumLiquidity.dmg`
- Windows: `src-tauri/target/release/bundle/msi/QuantumLiquidity.msi`
- Linux: `src-tauri/target/release/bundle/appimage/QuantumLiquidity.AppImage`

## Project Structure

```
desktop/
├── src/
│   ├── api/
│   │   ├── client.ts              # HTTP client
│   │   └── websocket.ts           # WebSocket + MessagePack
│   ├── components/
│   │   ├── Chart.tsx              # TradingView chart
│   │   ├── PositionTable.tsx      # Virtual scrolling table
│   │   └── MetricsCard.tsx        # Dashboard cards
│   ├── pages/
│   │   ├── Dashboard.tsx          # Main dashboard
│   │   ├── Positions.tsx          # Positions view
│   │   └── RiskMonitor.tsx        # Risk metrics
│   ├── hooks/
│   │   ├── useAPI.ts              # HTTP API hook
│   │   └── useWebSocket.ts        # WebSocket hook
│   ├── stores/
│   │   └── useStore.ts            # Zustand state
│   ├── types/
│   │   └── api.ts                 # TypeScript types
│   ├── App.tsx
│   └── main.tsx
├── src-tauri/
│   ├── src/
│   │   └── main.rs                # Tauri backend
│   ├── Cargo.toml
│   └── tauri.conf.json
├── package.json
├── tsconfig.json
└── vite.config.ts
```

## Dependencies

### Frontend
- `react` - UI library
- `react-router-dom` - Routing
- `lightweight-charts` - TradingView charts (NOT full TradingView)
- `msgpack-lite` - Binary serialization
- `zustand` - State management
- `react-window` - Virtual scrolling

### Build Tools
- `vite` - Fast bundler
- `typescript` - Type safety
- `@tauri-apps/cli` - Tauri CLI
- `@vitejs/plugin-react` - React plugin

## API Integration

### HTTP Client

```typescript
import { api } from '@/api/client'

// Get positions
const positions = await api.positions.list()

// Get risk metrics
const risk = await api.risk.metrics()
```

### WebSocket Client

```typescript
import { wsClient } from '@/api/websocket'

// Subscribe to EUR/USD ticks
wsClient.subscribe('ticks:EUR/USD')

// Handle messages
wsClient.onMessage((message) => {
  if (message.type === 'tick') {
    console.log('Tick:', message.instrument, message.bid, message.ask)
  }
})
```

## Optimization

### 1. Bundle Size
- Tree-shaking unused code
- Code splitting by route
- Lazy loading components
- WebP images, WOFF2 fonts

### 2. Runtime Performance
- Virtual scrolling for tables (react-window)
- Canvas-based charts (lightweight-charts)
- Binary WebSocket protocol (msgpack)
- Memoization with React.memo

### 3. Memory Management
- Unsubscribe from WebSocket topics on unmount
- Clear chart data on page change
- Limit table rows to visible area

## Testing

```bash
# Unit tests
npm test

# E2E tests
npm run test:e2e
```

## Deployment

### macOS
```bash
npm run tauri:build
# Output: src-tauri/target/release/bundle/dmg/QuantumLiquidity.dmg
# Size: ~15-20 MB
```

### Windows
```bash
npm run tauri:build
# Output: src-tauri/target/release/bundle/msi/QuantumLiquidity.msi
# Size: ~15-20 MB
```

### Linux
```bash
npm run tauri:build
# Output: src-tauri/target/release/bundle/appimage/QuantumLiquidity.AppImage
# Size: ~15-20 MB
```

## Current Status

✅ Project structure initialized
✅ Tauri configuration
✅ TypeScript types
✅ HTTP API client
✅ WebSocket client with MessagePack
🚧 React components
🚧 TradingView charts
🚧 Dashboard page
🚧 Positions page
🚧 Risk monitor page

**Progress:** ~30% complete

## Next Steps

1. Implement Dashboard with charts
2. Implement Positions table with virtual scrolling
3. Implement Risk monitor with alerts
4. Add strategy controls
5. Package and distribute

## Contributing

This is a proprietary project. See main README for contribution guidelines.
