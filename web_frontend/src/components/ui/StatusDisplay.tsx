import { useAppStore, selectSelectedRobot } from '../../store/appStore'

interface StatusDisplayProps {
  variant?: 'compact' | 'full' | 'bar'
  className?: string
}

export function StatusDisplay({ variant = 'compact', className = '' }: StatusDisplayProps) {
  const simTime = useAppStore((s) => s.simTime)
  const taskState = useAppStore((s) => s.taskState)
  const selectedRobot = useAppStore(selectSelectedRobot)
  const connected = useAppStore((s) => s.connected)

  const formatTime = (seconds: number) => {
    const mins = Math.floor(seconds / 60)
    const secs = Math.floor(seconds % 60)
    return `${mins.toString().padStart(2, '0')}:${secs.toString().padStart(2, '0')}`
  }

  if (variant === 'bar') {
    return (
      <div className={`flex items-center gap-6 text-sm ${className}`}>
        <div className="flex items-center gap-2">
          <span className="text-gray-500">Time:</span>
          <span className="font-mono text-gray-700">{formatTime(simTime)}</span>
        </div>
        {selectedRobot && (
          <>
            <div className="flex items-center gap-2">
              <span className="text-gray-500">Robot:</span>
              <span className="text-gray-700">{selectedRobot.id}</span>
            </div>
            <div className="flex items-center gap-2">
              <span className="text-gray-500">Pos:</span>
              <span className="font-mono text-gray-700">
                ({selectedRobot.x.toFixed(1)}, {selectedRobot.y.toFixed(1)})
              </span>
            </div>
          </>
        )}
        <div className="flex items-center gap-2">
          <span className="text-gray-500">State:</span>
          <span className="text-gray-700">{taskState}</span>
        </div>
      </div>
    )
  }

  if (variant === 'full') {
    return (
      <div className={`space-y-2 ${className}`}>
        <div className="flex justify-between text-sm">
          <span className="text-gray-500">Time</span>
          <span className="font-mono text-gray-700">{formatTime(simTime)}</span>
        </div>
        <div className="flex justify-between text-sm">
          <span className="text-gray-500">State</span>
          <span className="text-gray-700">{taskState}</span>
        </div>
        {selectedRobot && (
          <>
            <div className="flex justify-between text-sm">
              <span className="text-gray-500">Robot</span>
              <span className="text-gray-700">{selectedRobot.id}</span>
            </div>
            <div className="flex justify-between text-sm">
              <span className="text-gray-500">Position</span>
              <span className="font-mono text-gray-700">
                ({selectedRobot.x.toFixed(2)}, {selectedRobot.y.toFixed(2)})
              </span>
            </div>
            <div className="flex justify-between text-sm">
              <span className="text-gray-500">Carrying</span>
              <span className={selectedRobot.isCarrying ? 'text-amber-600' : 'text-gray-400'}>
                {selectedRobot.isCarrying ? 'Yes' : 'No'}
              </span>
            </div>
          </>
        )}
      </div>
    )
  }

  // Compact variant
  return (
    <div className={`flex items-center gap-3 ${className}`}>
      <span className="font-mono text-sm text-gray-600">{formatTime(simTime)}</span>
      <div className={`w-2 h-2 rounded-full ${connected ? 'bg-green-500' : 'bg-red-500'}`} />
    </div>
  )
}
