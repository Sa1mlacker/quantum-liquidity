/**
 * HTTP API Client
 * Handles REST requests to FastAPI backend
 */

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
  positions: {
    list: () => request('/positions'),
    summary: () => request('/positions/summary'),
    get: (instrument: string) => request(`/positions/${instrument}`),
  },

  // Orders
  orders: {
    list: (params?: { limit?: number; offset?: number; status?: string; instrument?: string }) => {
      const query = new URLSearchParams(params as any).toString()
      return request(`/orders${query ? `?${query}` : ''}`)
    },
    get: (orderId: number) => request(`/orders/${orderId}`),
    activeCount: () => request('/orders/active/count'),
  },

  // Trades
  trades: {
    list: (params?: { limit?: number; offset?: number; instrument?: string }) => {
      const query = new URLSearchParams(params as any).toString()
      return request(`/trades${query ? `?${query}` : ''}`)
    },
  },

  // Risk
  risk: {
    metrics: () => request('/risk/metrics'),
  },

  // Market Data
  market: {
    ticks: (instrument: string, limit = 100) =>
      request(`/market/ticks/${instrument}?limit=${limit}`),
    bars: (instrument: string, timeframe = '1m', limit = 500) =>
      request(`/market/bars/${instrument}?timeframe=${timeframe}&limit=${limit}`),
  },

  // Analytics
  analytics: {
    daily: (instrument: string, days = 30) =>
      request(`/analytics/daily/${instrument}?days=${days}`),
    orb: (instrument: string, period = 30, days = 30) =>
      request(`/analytics/orb/${instrument}?period=${period}&days=${days}`),
    volumeProfile: (instrument: string, days = 30) =>
      request(`/analytics/volume-profile/${instrument}?days=${days}`),
  },

  // Sentiment
  sentiment: {
    news: (instrument?: string, limit = 100) => {
      const query = new URLSearchParams({
        ...(instrument && { instrument }),
        limit: limit.toString(),
      }).toString()
      return request(`/sentiment/news?${query}`)
    },
    series: (instrument: string, limit = 100) =>
      request(`/sentiment/series/${instrument}?limit=${limit}`),
  },
}

export { APIError }
