import { ConnectionStatus } from './components/ConnectionStatus'
import { FpsCounter } from './components/FpsCounter'
import { MapPanel } from './components/panels/MapPanel'
import { ControlPanel } from './components/panels/ControlPanel'
import { ObjectivePanel } from './components/panels/ObjectivePanel'
import { StatusPanel } from './components/panels/StatusPanel'
import { RosConnectionProvider } from './hooks/useRosConnection'
import { RosDataBridge } from './components/RosDataBridge'
import { usePanelConfig } from './hooks/usePanelConfig'

function App() {
  const { config } = usePanelConfig()

  return (
    <RosConnectionProvider>
      <RosDataBridge />
      <div className="min-h-screen p-4">
        <header className="mb-4 flex justify-between items-center">
          <div className="flex items-center gap-4">
            <h1 className="text-2xl font-bold">Warehouser Simulation</h1>
            {config.showFps && <FpsCounter />}
          </div>
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
