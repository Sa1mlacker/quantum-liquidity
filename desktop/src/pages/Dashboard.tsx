/**
 * Dashboard Page
 * Main view: Charts + Metrics + Positions
 */

import { usePositionSummary, useBars, useRiskMetrics } from '@/hooks/useAPI'
import Chart from '@/components/Chart'
import MetricsCard from '@/components/MetricsCard'
import PositionTable from '@/components/PositionTable'

export default function Dashboard() {
  const { data: summary } = usePositionSummary()
  const { data: bars } = useBars('ES', '1m', 500)
  const { data: risk } = useRiskMetrics()

  if (!summary || !bars || !risk) {
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
