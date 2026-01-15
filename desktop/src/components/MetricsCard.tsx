/**
 * Metrics Card Component
 * Displays key trading metrics with live updates
 */

interface MetricsCardProps {
  label: string
  value: string | number
  change?: number
  format?: 'currency' | 'percent' | 'number'
}

export default function MetricsCard({
  label,
  value,
  change,
  format = 'number',
}: MetricsCardProps) {
  const formatValue = (val: string | number) => {
    if (typeof val === 'string') return val

    switch (format) {
      case 'currency':
        return `$${val.toFixed(2)}`
      case 'percent':
        return `${val.toFixed(2)}%`
      default:
        return val.toLocaleString()
    }
  }

  const changeColor = change && change > 0 ? '#26a69a' : change && change < 0 ? '#ef5350' : '#8b949e'
  const changeSign = change && change > 0 ? '+' : ''

  return (
    <div className="metrics-card">
      <div className="metrics-label">{label}</div>
      <div className="metrics-value">{formatValue(value)}</div>
      {change !== undefined && (
        <div className="metrics-change" style={{ color: changeColor }}>
          {changeSign}
          {change.toFixed(2)}%
        </div>
      )}
    </div>
  )
}
