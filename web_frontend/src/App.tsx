import { useEffect } from 'react'
import { Canvas } from './components/Canvas'
import { ControlPanel } from './components/ControlPanel'
import { ObjectivePanel } from './components/ObjectivePanel'
import { StatusPanel } from './components/StatusPanel'
import { useAppStore } from './store/appStore'
import { initRosConnection } from './ros/connection'

function App() {
  const connected = useAppStore((s) => s.connected)

  useEffect(() => {
    initRosConnection()
  }, [])

  return (
    <div className="min-h-screen p-4">
      <header className="mb-4">
        <h1 className="text-2xl font-bold">Warehouser Simulation</h1>
        <span className={`text-sm ${connected ? 'text-green-400' : 'text-red-400'}`}>
          {connected ? 'Connected' : 'Disconnected'}
        </span>
      </header>

      <div className="flex gap-4">
        <div className="flex-1">
          <Canvas />
        </div>

        <div className="w-64 space-y-4">
          <ControlPanel />
          <ObjectivePanel />
          <StatusPanel />
        </div>
      </div>
    </div>
  )
}

export default App
