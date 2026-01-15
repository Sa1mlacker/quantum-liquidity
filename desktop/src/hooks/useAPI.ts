/**
 * React hooks for API requests
 */

import { useState, useEffect } from 'react'
import { api } from '@/api/client'

interface UseQueryOptions {
  enabled?: boolean
  refetchInterval?: number
}

export function useQuery<T>(
  queryFn: () => Promise<T>,
  options: UseQueryOptions = {}
) {
  const { enabled = true, refetchInterval } = options

  const [data, setData] = useState<T | null>(null)
  const [loading, setLoading] = useState(false)
  const [error, setError] = useState<Error | null>(null)

  const refetch = async () => {
    if (!enabled) return

    setLoading(true)
    setError(null)

    try {
      const result = await queryFn()
      setData(result)
    } catch (err) {
      setError(err instanceof Error ? err : new Error('Unknown error'))
    } finally {
      setLoading(false)
    }
  }

  useEffect(() => {
    refetch()

    if (refetchInterval) {
      const interval = setInterval(refetch, refetchInterval)
      return () => clearInterval(interval)
    }
  }, [enabled, refetchInterval])

  return { data, loading, error, refetch }
}

// Specific hooks
export function usePositions() {
  return useQuery(() => api.positions.list(), { refetchInterval: 1000 })
}

export function usePositionSummary() {
  return useQuery(() => api.positions.summary(), { refetchInterval: 1000 })
}

export function useRiskMetrics() {
  return useQuery(() => api.risk.metrics(), { refetchInterval: 1000 })
}

export function useTrades(params?: { limit?: number; instrument?: string }) {
  return useQuery(() => api.trades.list(params), { refetchInterval: 5000 })
}

export function useBars(instrument: string, timeframe = '1m', limit = 500) {
  return useQuery(
    () => api.market.bars(instrument, timeframe, limit),
    { refetchInterval: 1000 }
  )
}

export function useHealth() {
  return useQuery(() => api.health(), { refetchInterval: 5000 })
}
