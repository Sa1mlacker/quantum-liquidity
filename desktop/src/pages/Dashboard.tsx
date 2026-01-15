/**
 * Dashboard Page
 * Main view: Charts + Metrics + Positions
 */

import { usePositionSummary, useBars, useRiskMetrics } from '@/hooks/useAPI'
import Chart from '@/components/Chart'
import MetricsCard from '@/components/MetricsCard'
import PositionTable from '@/components/PositionTable'

export default function Dashboard() {
  const { data: summary, loading: summaryLoading, error: summaryError } = usePositionSummary()
  const { data: bars, loading: barsLoading, error: barsError } = useBars('ES', '1m', 500)
  const { data: risk, loading: riskLoading, error: riskError } = useRiskMetrics()

  const hasError = summaryError || barsError || riskError
  const isLoading = summaryLoading || barsLoading || riskLoading

  if (hasError) {
    return (
      <div className="error-screen">
        <h2>Backend Connection Error</h2>
        <p>Cannot connect to QuantumLiquidity API server.</p>
        <p>Please ensure the Python API server is running:</p>
        <pre>cd python && uvicorn quantumliquidity.api.main:app --reload</pre>
        <p className="error-details">Error: {(summaryError || barsError || riskError)?.message}</p>
      </div>
    )
  }

  if (isLoading || !summary || !bars || !risk) {
    return <div className="loading">Loading...</div>
  }

  return (
    <div className="dashboard">
      <header className="dashboard-header">
        <h1>QuantumLiquidity</h1>
        <div className="status">
          {risk.halt_active ? (
            <span className="status-halted">TRADING HALTED</span>
          ) : (
            <span className="status-active">ACTIVE</span>
          )}
        </div>
      </header>

      <div className="metrics-grid">
        <MetricsCard
          label="Daily PnL"
          value={risk.daily_pnl}
          format="currency"
        />
        <MetricsCard
          label="Unrealized PnL"
          value={summary.total_unrealized_pnl}
          format="currency"
        />
        <MetricsCard
          label="Realized PnL"
          value={summary.total_realized_pnl}
          format="currency"
        />
        <MetricsCard
          label="Total Exposure"
          value={summary.total_exposure}
          format="currency"
        />
        <MetricsCard
          label="Positions"
          value={summary.total_positions}
        />
        <MetricsCard
          label="Account Utilization"
          value={risk.account_utilization}
          format="percent"
        />
      </div>

      <div className="chart-section">
        <h2>ES - E-mini S&P 500</h2>
        <Chart data={bars} height={400} />
      </div>

      <div className="positions-section">
        <h2>Active Positions</h2>
        <PositionTable positions={summary.positions} height={300} />
      </div>

      {risk.halt_reason && (
        <div className="alert-banner">
          ⚠️ Trading halted: {risk.halt_reason}
        </div>
      )}
    </div>
  )
}
