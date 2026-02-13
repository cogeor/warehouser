import { useAppStore } from '../../store/appStore'
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
  const entities = useAppStore((s) => s.entities)
  const { connectionError, reconnectAttempt, isConnected, retryConnection } = useRosConnection()

  const robot = entities.find((e) => e.type === 'robot')

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
        {robot && (
          <>
            <div className="flex justify-between">
              <span className="text-gray-400">Position:</span>
              <span>
                ({robot.x.toFixed(2)}, {robot.y.toFixed(2)})
              </span>
            </div>
            <div className="flex justify-between">
              <span className="text-gray-400">Carrying:</span>
              <span className={robot.isCarrying ? 'text-yellow-400' : 'text-gray-500'}>
                {robot.isCarrying ? 'Yes' : 'No'}
              </span>
            </div>
          </>
        )}
      </div>
    </div>
  )
}
