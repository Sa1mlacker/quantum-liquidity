/**
 * Mock data for development/demo mode
 * Used when backend API is not available
 */

export const mockData = {
  health: {
    status: 'ok',
    version: '1.0.0',
    timestamp: new Date().toISOString()
  },

  positionSummary: {
    total_positions: 3,
    total_exposure: 125000,
    total_pnl: 2450.75,
    total_pnl_pct: 1.96,
    winning_positions: 2,
    losing_positions: 1
  },

  positions: [
    {
      id: 1,
      instrument: 'EUR/USD',
      side: 'long',
      size: 50000,
      entry_price: 1.0850,
      current_price: 1.0875,
      pnl: 1250.00,
      pnl_pct: 2.3,
      opened_at: '2025-01-15T10:30:00Z'
    },
    {
      id: 2,
      instrument: 'GBP/USD',
      side: 'long',
      size: 40000,
      entry_price: 1.2650,
      current_price: 1.2680,
      pnl: 1200.75,
      pnl_pct: 2.37,
      opened_at: '2025-01-15T11:15:00Z'
    },
    {
      id: 3,
      instrument: 'USD/JPY',
      side: 'short',
      size: 35000,
      entry_price: 148.50,
      current_price: 148.60,
      pnl: -35.00,
      pnl_pct: -0.07,
      opened_at: '2025-01-15T12:00:00Z'
    }
  ],

  riskMetrics: {
    var_95: 2450.00,
    cvar_95: 3200.00,
    sharpe_ratio: 1.85,
    max_drawdown: -1250.00,
    max_drawdown_pct: -2.5,
    win_rate: 0.67,
    profit_factor: 2.45,
    total_trades: 127,
    winning_trades: 85,
    losing_trades: 42
  },

  bars: (instrument: string) => {
    const now = Date.now()
    const bars = []
    for (let i = 500; i > 0; i--) {
      const basePrice = instrument === 'EUR/USD' ? 1.0850 : 
                       instrument === 'GBP/USD' ? 1.2650 : 148.50
      const volatility = basePrice * 0.0002
      const time = now - (i * 60000) // 1 minute bars
      
      bars.push({
        time: new Date(time).toISOString(),
        open: basePrice + (Math.random() - 0.5) * volatility,
        high: basePrice + Math.random() * volatility,
        low: basePrice - Math.random() * volatility,
        close: basePrice + (Math.random() - 0.5) * volatility,
        volume: Math.floor(Math.random() * 10000) + 1000
      })
    }
    return bars
  },

  trades: {
    trades: [
      {
        id: 101,
        instrument: 'EUR/USD',
        side: 'long',
        size: 25000,
        entry_price: 1.0850,
        exit_price: 1.0875,
        pnl: 625.00,
        commission: 5.00,
        opened_at: '2025-01-15T09:00:00Z',
        closed_at: '2025-01-15T10:30:00Z',
        duration_seconds: 5400
      },
      {
        id: 102,
        instrument: 'GBP/USD',
        side: 'short',
        size: 30000,
        entry_price: 1.2700,
        exit_price: 1.2680,
        pnl: 600.00,
        commission: 6.00,
        opened_at: '2025-01-15T08:00:00Z',
        closed_at: '2025-01-15T09:30:00Z',
        duration_seconds: 5400
      }
    ],
    total: 127,
    page: 1,
    per_page: 20
  },

  orders: {
    orders: [
      {
        id: 'ORD-001',
        instrument: 'EUR/USD',
        side: 'long',
        type: 'limit',
        size: 50000,
        price: 1.0850,
        status: 'filled',
        filled_size: 50000,
        created_at: '2025-01-15T10:30:00Z',
        updated_at: '2025-01-15T10:30:05Z'
      }
    ],
    total: 3
  }
}
