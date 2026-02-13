import { useAppStore, selectRobots, selectSelectedRobot } from '../../store/appStore'
import { useRosConnection } from '../../hooks/useRosConnection'
import type { PanelConfig, PanelProps } from '../../types/panels'

export const statusPanelConfig: PanelConfig = {
  id: 'status',
  title: 'Status',
  defaultVisible: true,
}

export function StatusPanel({ isCollapsed }: PanelProps) {
  const taskState = useAppStore((s) => s.taskState)
  const taskIntent = useAppStore((s) => s.taskIntent)
  const simTime = useAppStore((s) => s.simTime)
  const robots = useAppStore(selectRobots)
  const selectedRobot = useAppStore(selectSelectedRobot)
  const setSelectedRobotId = useAppStore((s) => s.setSelectedRobotId)
  const { connectionError, reconnectAttempt, isConnected, retryConnection } = useRosConnection()

  const stateColors: Record<string, string> = {
    IDLE: 'text-gray-400',
    NAVIGATING_TO_PICK: 'text-blue-400',
    PICKING: 'text-yellow-400',
    NAVIGATING_TO_PLACE: 'text-blue-400',
    PLACING: 'text-yellow-400',
    COMPLETED: 'text-green-400',
    FAILED: 'text-red-400',
    CANCELLED: 'text-orange-400',
  }

  const handleRetry = () => {
    retryConnection()
  }

  if (isCollapsed) {
    return null
  }

  return (
    <div className="bg-gray-800 rounded-lg p-4">
      <h2 className="text-lg font-semibold mb-3">Status</h2>

      {/* Robot Selector */}
      {robots.length > 1 && (
        <div className="mb-3">
          <label className="text-sm text-gray-400">Select Robot:</label>
          <select
            value={selectedRobot?.id ?? ''}
            onChange={(e) => setSelectedRobotId(e.target.value)}
            className="w-full bg-gray-700 text-white py-1 px-2 rounded mt-1"
          >
            {robots.map((r) => (
              <option key={r.id} value={r.id}>{r.id}</option>
            ))}
          </select>
        </div>
      )}

      {/* Connection Error Display */}
      {connectionError && (
        <div
          className="mb-3 p-3 bg-red-900/50 border border-red-600 rounded cursor-pointer hover:bg-red-900/70 transition-colors"
          onClick={handleRetry}
          role="button"
          tabIndex={0}
          onKeyDown={(e) => {
            if (e.key === 'Enter' || e.key === ' ') {
              handleRetry()
            }
          }}
        >
          <div className="text-red-400 font-medium">Connection Failed</div>
          <div className="text-red-300 text-sm mt-1">{connectionError}</div>
        </div>
      )}

      {/* Reconnecting indicator */}
      {!isConnected && !connectionError && reconnectAttempt > 0 && (
        <div className="mb-3 p-3 bg-yellow-900/50 border border-yellow-600 rounded">
          <div className="text-yellow-400 font-medium">Reconnecting...</div>
          <div className="text-yellow-300 text-sm mt-1">
            Attempt {reconnectAttempt} of 10
          </div>
        </div>
      )}

      <div className="space-y-2 text-sm">
        <div className="flex justify-between">
          <span className="text-gray-400">State:</span>
          <span className={stateColors[taskState] || 'text-white'}>{taskState}</span>
        </div>
        {taskIntent && (
          <div className="flex justify-between">
            <span className="text-gray-400">Intent:</span>
            <span>{taskIntent}</span>
          </div>
        )}
        <div className="flex justify-between">
          <span className="text-gray-400">Time:</span>
          <span>{simTime.toFixed(2)}s</span>
        </div>
        {selectedRobot && (
          <>
            <div className="flex justify-between">
              <span className="text-gray-400">Position:</span>
              <span>
                ({selectedRobot.x.toFixed(2)}, {selectedRobot.y.toFixed(2)})
              </span>
            </div>
            <div className="flex justify-between">
              <span className="text-gray-400">Carrying:</span>
              <span className={selectedRobot.isCarrying ? 'text-yellow-400' : 'text-gray-500'}>
                {selectedRobot.isCarrying ? 'Yes' : 'No'}
              </span>
            </div>
          </>
        )}
      </div>
    </div>
  )
}
