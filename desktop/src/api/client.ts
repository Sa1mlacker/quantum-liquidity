/**
 * HTTP API Client
 * Handles REST requests to FastAPI backend
 */

import { mockData } from './mockData'

// Enable mock data mode when API is unavailable
let USE_MOCK_DATA = false

const API_BASE = 'http://localhost:8000/api/v1'

class APIError extends Error {
  constructor(
    message: string,
    public status: number,
    public data?: any
  ) {
    super(message)
    this.name = 'APIError'
  }
}

async function request<T>(
  endpoint: string,
  options: RequestInit = {}
): Promise<T> {
  const url = `${API_BASE}${endpoint}`

  try {
    const response = await fetch(url, {
      ...options,
      headers: {
        'Content-Type': 'application/json',
        ...options.headers,
      },
    })

    if (!response.ok) {
      const errorData = await response.json().catch(() => ({}))
      throw new APIError(
        errorData.error || 'Request failed',
        response.status,
        errorData
      )
    }

    return await response.json()
  } catch (error) {
        // Enable mock data mode when API is unavailable
    USE_MOCK_DATA = true
    console.warn('API unavailable, switching to mock data mode')
    if (error instanceof APIError) {
      throw error
    }
    throw new APIError(
      `Network error: ${error instanceof Error ? error.message : 'Unknown'}`,
      0
    )
  }
}

export const api = {
  // Health
  health: () => request('/health'),

  // Positions
    // Health
  health: async () => {
    if (USE_MOCK_DATA) return mockData.health
    return request('/health')
  },

  // Positions
  positions: {
    list: async () => {
      if (USE_MOCK_DATA) return mockData.positions
      return request('/positions')
    },
    summary: async () => {
      if (USE_MOCK_DATA) return mockData.positionSummary
      return request('/positions/summary')
    },
    get: async (instrument: string) => {
      if (USE_MOCK_DATA) {
        return mockData.positions.find(p => p.instrument === instrument) || null
      }
      return request(`/positions/${instrument}`)
    },
  },

  // Risk
  risk: {
    metrics: async () => {
      if (USE_MOCK_DATA) return mockData.riskMetrics
      return request('/risk/metrics')
    },
  },

  // Market
  market: {
    bars: async (instrument: string, timeframe = '1m', limit = 500) => {
      if (USE_MOCK_DATA) return mockData.bars(instrument)
      const query = new URLSearchParams({ timeframe, limit: limit.toString() })
      return request(`/market/${instrument}/bars?${query}`)
    },
  },

  // Trades
  trades: {
    list: async (params?: { limit?: number; instrument?: string }) => {
      if (USE_MOCK_DATA) return mockData.trades
      const query = params ? new URLSearchParams(params as any).toString() : ''
      return request(`/trades?${query}`)
    },
  },

  // Orders
  orders: {
    list: async (params?: { limit?: number; offset?: number; status?: string; instrument?: string }) => {
      if (USE_MOCK_DATA) return mockData.orders
      const query = params ? new URLSearchParams(params as any).toString() : ''
      return request(`/orders?${query}`)
    },
    get: async (orderId: number) => {
      if (USE_MOCK_DATA) {
        return mockData.orders.orders.find(o => o.id === orderId.toString()) || null
      }
      return request(`/orders/${orderId}`)
    },
    activeCount: async () => {
      if (USE_MOCK_DATA) return { count: mockData.orders.total }
      return request('/orders/active/count')
    },
  },
  }
