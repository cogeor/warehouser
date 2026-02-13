import { useEffect } from 'react'
import { Canvas } from './components/Canvas'
import { ControlPanel } from './components/ControlPanel'
import { ObjectivePanel } from './components/ObjectivePanel'
import { StatusPanel } from './components/StatusPanel'
import { useAppStore } from './store/appStore'
import { RosConnectionProvider, useRosConnection } from './hooks/useRosConnection'

/**
 * Inner app content component that uses ROS connection context.
 * Syncs connection state to appStore for backwards compatibility.
 */
function AppContent() {
  const {
    isConnected,
    connectionError,
    reconnectAttempt,
    maxReconnectAttempts,
    retryConnection,
  } = useRosConnection()

  const connected = useAppStore((s) => s.connected)
  const setConnected = useAppStore((s) => s.setConnected)

  // Sync connection state to appStore for backwards compatibility
  useEffect(() => {
    setConnected(isConnected)
  }, [isConnected, setConnected])

  // Check if max reconnect attempts reached
  const maxAttemptsReached = reconnectAttempt >= maxReconnectAttempts && maxReconnectAttempts > 0

  return (
    <div className="min-h-screen p-4">
      <header className="mb-4">
        <h1 className="text-2xl font-bold">Warehouser Simulation</h1>
        <span className={`text-sm ${connected ? 'text-green-400' : 'text-red-400'}`}>
          {connected ? 'Connected' : 'Disconnected'}
        </span>
      </header>

      {maxAttemptsReached && connectionError && (
        <div className="mb-4 p-4 bg-red-900/50 border border-red-500 rounded">
          <p className="text-red-300 mb-2">Connection failed: {connectionError}</p>
          <button
            onClick={retryConnection}
            className="px-4 py-2 bg-red-600 hover:bg-red-500 text-white rounded"
          >
            Retry Connection
          </button>
        </div>
      )}

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

function App() {
  return (
    <RosConnectionProvider>
      <AppContent />
    </RosConnectionProvider>
  )
}

export default App
