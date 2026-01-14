import { BrowserRouter, Routes, Route, Navigate } from 'react-router-dom'
import Dashboard from './pages/Dashboard'
import Positions from './pages/Positions'
import RiskMonitor from './pages/RiskMonitor'
import './App.css'

function App() {
  return (
    <BrowserRouter>
      <div className="app">
        <Routes>
          <Route path="/" element={<Navigate to="/dashboard" replace />} />
          <Route path="/dashboard" element={<Dashboard />} />
          <Route path="/positions" element={<Positions />} />
          <Route path="/risk" element={<RiskMonitor />} />
        </Routes>
      </div>
    </BrowserRouter>
  )
}

export default App
