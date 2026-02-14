/**
 * Main application layout.
 *
 * Left sidebar with controls/status, canvas centered on right.
 * Professional simulation layout inspired by ROS Rviz.
 */

import { SimulationCanvas } from '../components/SimulationCanvas'
import { StatusDisplay } from '../components/ui/StatusDisplay'
import { ConnectionDot } from '../components/ui/ConnectionDot'
import { useTriggerService } from '../hooks/useRosService'
import { useAppStore } from '../store/appStore'
import { useKeyboardControl } from '../hooks/useKeyboardControl'

export function LayoutSplit() {
  // Enable keyboard control
  useKeyboardControl()

  return (
    <div className="min-h-screen bg-white flex">
      {/* Left sidebar */}
      <aside className="w-56 bg-white border-r border-gray-200 flex flex-col">
        {/* Header */}
        <div className="px-4 py-3 border-b border-gray-100">
          <div className="flex items-center justify-between">
            <span className="font-semibold text-gray-800">Warehouser</span>
            <ConnectionDot />
          </div>
        </div>

        {/* Controls section */}
        <div className="p-4 border-b border-gray-100">
          <h3 className="text-xs font-medium text-gray-500 uppercase tracking-wide mb-3">
            Controls
          </h3>
          <div className="space-y-2">
            <ControlButton label="Run" icon="play" />
            <ControlButton label="Pause" icon="pause" />
            <ControlButton label="Reset" icon="reset" />
          </div>
        </div>

        {/* Keyboard controls hint */}
        <div className="p-4 border-b border-gray-100">
          <h3 className="text-xs font-medium text-gray-500 uppercase tracking-wide mb-3">
            Drive Robot
          </h3>
          <div className="text-xs text-gray-500 space-y-1">
            <div className="flex gap-2">
              <kbd className="px-1.5 py-0.5 bg-gray-100 rounded text-gray-700">W</kbd>
              <span>Forward</span>
            </div>
            <div className="flex gap-2">
              <kbd className="px-1.5 py-0.5 bg-gray-100 rounded text-gray-700">S</kbd>
              <span>Backward</span>
            </div>
            <div className="flex gap-2">
              <kbd className="px-1.5 py-0.5 bg-gray-100 rounded text-gray-700">A</kbd>
              <span>Turn left</span>
            </div>
            <div className="flex gap-2">
              <kbd className="px-1.5 py-0.5 bg-gray-100 rounded text-gray-700">D</kbd>
              <span>Turn right</span>
            </div>
          </div>
        </div>

        {/* Status section */}
        <div className="p-4 flex-1">
          <h3 className="text-xs font-medium text-gray-500 uppercase tracking-wide mb-3">
            Status
          </h3>
          <StatusDisplay variant="full" />
        </div>
      </aside>

      {/* Main canvas area */}
      <main className="flex-1 flex items-center justify-center p-6 bg-gray-50">
        <SimulationCanvas />
      </main>
    </div>
  )
}

function ControlButton({ label, icon }: { label: string; icon: 'play' | 'pause' | 'reset' }) {
  const { startSim, pauseSim, resetSim, simRunning, setSimRunning } = useSimControls()

  const handleClick = async () => {
    if (icon === 'play') {
      const success = await startSim()
      if (success) setSimRunning(true)
    } else if (icon === 'pause') {
      const success = await pauseSim()
      if (success) setSimRunning(false)
    } else {
      await resetSim()
      setSimRunning(false)
    }
  }

  const isActive = icon === 'play' ? !simRunning : icon === 'pause' ? simRunning : false
  const isDisabled = icon === 'play' ? simRunning : icon === 'pause' ? !simRunning : false

  return (
    <button
      onClick={handleClick}
      disabled={isDisabled}
      className={`w-full px-3 py-2 rounded text-left text-sm transition-colors ${
        isDisabled
          ? 'text-gray-300 cursor-not-allowed'
          : isActive
          ? 'bg-gray-100 text-gray-900'
          : 'text-gray-600 hover:bg-gray-50'
      }`}
    >
      <span className="flex items-center gap-2">
        <ControlIcon icon={icon} />
        {label}
      </span>
    </button>
  )
}

function ControlIcon({ icon }: { icon: 'play' | 'pause' | 'reset' }) {
  const className = 'w-4 h-4'

  if (icon === 'play') {
    return (
      <svg className={className} fill="currentColor" viewBox="0 0 24 24">
        <path d="M8 5v14l11-7z" />
      </svg>
    )
  }
  if (icon === 'pause') {
    return (
      <svg className={className} fill="currentColor" viewBox="0 0 24 24">
        <rect x="6" y="4" width="4" height="16" rx="1" />
        <rect x="14" y="4" width="4" height="16" rx="1" />
      </svg>
    )
  }
  return (
    <svg className={className} fill="none" stroke="currentColor" viewBox="0 0 24 24">
      <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M4 4v5h.582m15.356 2A8.001 8.001 0 004.582 9m0 0H9m11 11v-5h-.581m0 0a8.003 8.003 0 01-15.357-2m15.357 2H15" />
    </svg>
  )
}

// Hook for simulation controls
function useSimControls() {
  const simRunning = useAppStore((s) => s.simRunning)
  const setSimRunning = useAppStore((s) => s.setSimRunning)
  const startSim = useTriggerService('/sim/start')
  const pauseSim = useTriggerService('/sim/pause')
  const resetSim = useTriggerService('/sim/reset')

  return { startSim, pauseSim, resetSim, simRunning, setSimRunning }
}
