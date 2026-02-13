import { ConnectionStatus } from './components/ConnectionStatus'
import { MapPanel } from './components/panels/MapPanel'
import { ControlPanel } from './components/panels/ControlPanel'
import { ObjectivePanel } from './components/panels/ObjectivePanel'
import { StatusPanel } from './components/panels/StatusPanel'
import { RosConnectionProvider } from './hooks/useRosConnection'

function App() {
  return (
    <RosConnectionProvider>
      <div className="min-h-screen p-4">
        <header className="mb-4 flex justify-between items-center">
          <h1 className="text-2xl font-bold">Warehouser Simulation</h1>
          <ConnectionStatus />
        </header>

        <div className="flex gap-4">
          <div className="flex-1">
            <MapPanel />
          </div>

          <div className="w-64 space-y-4">
            <ControlPanel />
            <ObjectivePanel />
            <StatusPanel />
          </div>
        </div>
      </div>
    </RosConnectionProvider>
  )
}

export default App
