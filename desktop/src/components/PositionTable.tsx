/**
 * Position Table with Virtual Scrolling
 * Optimized: Only renders visible rows
 */

import { FixedSizeList as List } from 'react-window'
import { Position } from '@/types/api'

interface PositionTableProps {
  positions: Position[]
  height?: number
}

export default function PositionTable({ positions, height = 400 }: PositionTableProps) {
  const Row = ({ index, style }: { index: number; style: React.CSSProperties }) => {
    const pos = positions[index]
    const pnlColor = pos.unrealized_pnl >= 0 ? '#26a69a' : '#ef5350'
    const sideColor = pos.quantity > 0 ? '#26a69a' : '#ef5350'

    return (
      <div className="position-row" style={style}>
        <div className="position-cell">{pos.instrument}</div>
        <div className="position-cell" style={{ color: sideColor }}>
          {pos.quantity > 0 ? 'LONG' : 'SHORT'} {Math.abs(pos.quantity)}
        </div>
        <div className="position-cell">${pos.entry_price.toFixed(2)}</div>
        <div className="position-cell" style={{ color: pnlColor }}>
          ${pos.unrealized_pnl.toFixed(2)}
        </div>
        <div className="position-cell">${pos.realized_pnl.toFixed(2)}</div>
        <div className="position-cell">{pos.num_fills_today}</div>
        <div className="position-cell">${pos.total_commission.toFixed(2)}</div>
      </div>
    )
  }

  return (
    <div className="position-table">
      <div className="position-header">
        <div className="position-cell">Instrument</div>
        <div className="position-cell">Side/Qty</div>
        <div className="position-cell">Entry Price</div>
        <div className="position-cell">Unrealized PnL</div>
        <div className="position-cell">Realized PnL</div>
        <div className="position-cell">Fills</div>
        <div className="position-cell">Commission</div>
      </div>
      <List
        height={height}
        itemCount={positions.length}
        itemSize={50}
        width="100%"
      >
        {Row}
      </List>
    </div>
  )
}
