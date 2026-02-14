import { useTriggerService } from '../../hooks/useRosService'
import { useAppStore } from '../../store/appStore'

interface TransportControlsProps {
  variant?: 'minimal' | 'labeled' | 'icon-only'
  className?: string
}

export function TransportControls({ variant = 'labeled', className = '' }: TransportControlsProps) {
  const simRunning = useAppStore((s) => s.simRunning)
  const setSimRunning = useAppStore((s) => s.setSimRunning)

  const startSim = useTriggerService('/sim/start')
  const pauseSim = useTriggerService('/sim/pause')
  const resetSim = useTriggerService('/sim/reset')

  const handlePlayPause = async () => {
    if (simRunning) {
      const success = await pauseSim()
      if (success) setSimRunning(false)
    } else {
      const success = await startSim()
      if (success) setSimRunning(true)
    }
  }

  const handleReset = async () => {
    await resetSim()
    setSimRunning(false)
  }

  const buttonBase = 'flex items-center justify-center transition-colors'

  const buttonStyles = {
    minimal: `${buttonBase} w-8 h-8 rounded-full hover:bg-gray-100`,
    labeled: `${buttonBase} px-3 py-1.5 rounded hover:bg-gray-100`,
    'icon-only': `${buttonBase} w-10 h-10 rounded-lg hover:bg-gray-100`,
  }

  const iconSize = variant === 'minimal' ? 'w-4 h-4' : 'w-5 h-5'

  return (
    <div className={`flex items-center gap-1 ${className}`}>
      {/* Play/Pause */}
      <button
        onClick={handlePlayPause}
        className={buttonStyles[variant]}
        title={simRunning ? 'Pause' : 'Play'}
      >
        {simRunning ? (
          <svg className={`${iconSize} text-gray-600`} fill="currentColor" viewBox="0 0 24 24">
            <rect x="6" y="4" width="4" height="16" rx="1" />
            <rect x="14" y="4" width="4" height="16" rx="1" />
          </svg>
        ) : (
          <svg className={`${iconSize} text-gray-600`} fill="currentColor" viewBox="0 0 24 24">
            <path d="M8 5v14l11-7z" />
          </svg>
        )}
        {variant === 'labeled' && (
          <span className="ml-1.5 text-sm text-gray-700">{simRunning ? 'Pause' : 'Play'}</span>
        )}
      </button>

      {/* Reset */}
      <button
        onClick={handleReset}
        className={buttonStyles[variant]}
        title="Reset"
      >
        <svg className={`${iconSize} text-gray-600`} fill="none" stroke="currentColor" viewBox="0 0 24 24">
          <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M4 4v5h.582m15.356 2A8.001 8.001 0 004.582 9m0 0H9m11 11v-5h-.581m0 0a8.003 8.003 0 01-15.357-2m15.357 2H15" />
        </svg>
        {variant === 'labeled' && (
          <span className="ml-1.5 text-sm text-gray-700">Reset</span>
        )}
      </button>
    </div>
  )
}
