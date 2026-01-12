import { useAppStore } from '../store/appStore'

export function StatusPanel() {
  const taskState = useAppStore((s) => s.taskState)
  const taskIntent = useAppStore((s) => s.taskIntent)
  const simTime = useAppStore((s) => s.simTime)
  const entities = useAppStore((s) => s.entities)

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

  return (
    <div className="bg-gray-800 rounded-lg p-4">
      <h2 className="text-lg font-semibold mb-3">Status</h2>
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
