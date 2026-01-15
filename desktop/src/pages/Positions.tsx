/**
 * Positions Page
 * Detailed view of all positions
 */

import { usePositions, useTrades } from '@/hooks/useAPI'
import PositionTable from '@/components/PositionTable'

export default function Positions() {
  const { data: positions, loading: posLoading } = usePositions()
  const { data: trades, loading: tradesLoading } = useTrades({ limit: 50 })

  if (posLoading || tradesLoading) {
    return <div className="loading">Loading...</div>
  }

  return (
    <div className="positions-page">
      <header>
        <h1>Positions</h1>
      </header>

      <section className="positions-section">
        <h2>Current Positions</h2>
        {positions && positions.length > 0 ? (
          <PositionTable positions={positions} height={400} />
        ) : (
          <div className="empty-state">No open positions</div>
        )}
      </section>

      <section className="trades-section">
        <h2>Recent Trades</h2>
        {trades && trades.length > 0 ? (
          <div className="trades-table">
            <div className="trades-header">
              <div className="trade-cell">Time</div>
              <div className="trade-cell">Instrument</div>
              <div className="trade-cell">Side</div>
              <div className="trade-cell">Qty</div>
              <div className="trade-cell">Price</div>
              <div className="trade-cell">PnL</div>
              <div className="trade-cell">Commission</div>
            </div>
            {trades.map((trade: any) => (
              <div key={trade.trade_id} className="trade-row">
                <div className="trade-cell">
                  {new Date(trade.timestamp).toLocaleTimeString()}
                </div>
                <div className="trade-cell">{trade.instrument}</div>
                <div
                  className="trade-cell"
                  style={{ color: trade.side === 'BUY' ? '#26a69a' : '#ef5350' }}
                >
                  {trade.side}
                </div>
                <div className="trade-cell">{trade.quantity}</div>
                <div className="trade-cell">${trade.price.toFixed(2)}</div>
                <div
                  className="trade-cell"
                  style={{
                    color:
                      trade.pnl > 0 ? '#26a69a' : trade.pnl < 0 ? '#ef5350' : '#8b949e',
                  }}
                >
                  {trade.pnl ? `$${trade.pnl.toFixed(2)}` : '-'}
                </div>
                <div className="trade-cell">${trade.commission.toFixed(2)}</div>
              </div>
            ))}
          </div>
        ) : (
          <div className="empty-state">No trades today</div>
        )}
      </section>
    </div>
  )
}
