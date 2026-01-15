/**
 * Risk Monitor Page
 * Real-time risk metrics and alerts
 */

import { useRiskMetrics } from '@/hooks/useAPI'
import MetricsCard from '@/components/MetricsCard'

export default function RiskMonitor() {
  const { data: risk, loading } = useRiskMetrics()

  if (loading || !risk) {
    return <div className="loading">Loading...</div>
  }

  const utilizationColor =
    risk.account_utilization > 80
      ? '#ef5350'
      : risk.account_utilization > 60
      ? '#ff9800'
      : '#26a69a'

  const ddColor = Math.abs(risk.max_dd_today) > 1000 ? '#ef5350' : '#8b949e'

  return (
    <div className="risk-page">
      <header>
        <h1>Risk Monitor</h1>
        <div className="timestamp">
          Last update: {new Date(risk.timestamp).toLocaleTimeString()}
        </div>
      </header>

      {risk.halt_active && (
        <div className="alert-critical">
          <h2>⚠️ TRADING HALTED</h2>
          <p>{risk.halt_reason}</p>
        </div>
      )}

      <section className="risk-metrics">
        <h2>Account Metrics</h2>
        <div className="metrics-grid">
          <MetricsCard
            label="Account Utilization"
            value={risk.account_utilization}
            format="percent"
          />
          <MetricsCard
            label="Total Exposure"
            value={risk.total_exposure}
            format="currency"
          />
          <MetricsCard
            label="Daily PnL"
            value={risk.daily_pnl}
            format="currency"
          />
          <MetricsCard
            label="Max Drawdown Today"
            value={risk.max_dd_today}
            format="currency"
          />
          <MetricsCard
            label="Daily High PnL"
            value={risk.daily_high_pnl}
            format="currency"
          />
          <MetricsCard
            label="Realized PnL"
            value={risk.realized_pnl}
            format="currency"
          />
          <MetricsCard
            label="Unrealized PnL"
            value={risk.unrealized_pnl}
            format="currency"
          />
        </div>
      </section>

      <section className="order-stats">
        <h2>Order Statistics</h2>
        <div className="metrics-grid">
          <MetricsCard label="Submitted" value={risk.orders_submitted_today} />
          <MetricsCard label="Filled" value={risk.orders_filled_today} />
          <MetricsCard label="Rejected" value={risk.orders_rejected_today} />
          <MetricsCard label="Cancelled" value={risk.orders_cancelled_today} />
        </div>
      </section>

      <section className="risk-indicators">
        <h2>Risk Indicators</h2>
        <div className="indicator-grid">
          <div className="indicator">
            <div className="indicator-label">Account Utilization</div>
            <div className="progress-bar">
              <div
                className="progress-fill"
                style={{
                  width: `${risk.account_utilization}%`,
                  backgroundColor: utilizationColor,
                }}
              />
            </div>
            <div className="indicator-value">{risk.account_utilization.toFixed(1)}%</div>
          </div>

          <div className="indicator">
            <div className="indicator-label">Drawdown from High</div>
            <div className="progress-bar">
              <div
                className="progress-fill"
                style={{
                  width: `${Math.min(
                    100,
                    (Math.abs(risk.max_dd_today) / risk.daily_high_pnl) * 100
                  )}%`,
                  backgroundColor: ddColor,
                }}
              />
            </div>
            <div className="indicator-value">${risk.max_dd_today.toFixed(2)}</div>
          </div>
        </div>
      </section>

      <section className="risk-warnings">
        <h2>Active Warnings</h2>
        {risk.halt_active ? (
          <div className="warning-item critical">
            <span className="warning-icon">🛑</span>
            <span>Trading Halted: {risk.halt_reason}</span>
          </div>
        ) : risk.account_utilization > 80 ? (
          <div className="warning-item high">
            <span className="warning-icon">⚠️</span>
            <span>Account utilization above 80%</span>
          </div>
        ) : risk.account_utilization > 60 ? (
          <div className="warning-item medium">
            <span className="warning-icon">⚡</span>
            <span>Account utilization above 60%</span>
          </div>
        ) : (
          <div className="warning-item ok">
            <span className="warning-icon">✅</span>
            <span>All systems nominal</span>
          </div>
        )}
      </section>
    </div>
  )
}
