import { useAppStore } from '../../store/appStore'
import type { PanelConfig, PanelProps } from '../../types/panels'

export const optionsPanelConfig: PanelConfig = {
  id: 'options',
  title: 'Options',
  defaultVisible: true,
}

export function OptionsPanel({ isCollapsed }: PanelProps) {
  const traceEnabled = useAppStore((s) => s.traceEnabled)
  const setTraceEnabled = useAppStore((s) => s.setTraceEnabled)
  const trajectoryHistory = useAppStore((s) => s.trajectoryHistory)
  const clearTrajectory = useAppStore((s) => s.clearTrajectory)

  const handleTraceToggle = () => {
    setTraceEnabled(!traceEnabled)
  }

  const handleClear = () => {
    clearTrajectory()
  }

  if (isCollapsed) {
    return null
  }

  return (
    <div className="space-y-3">
      {/* Trajectory Trace section */}
      <div className="flex items-center justify-between">
        <label className="flex items-center gap-2 text-sm text-gray-600 cursor-pointer">
          <input
            type="checkbox"
            checked={traceEnabled}
            onChange={handleTraceToggle}
            className="w-4 h-4 rounded border-gray-300 text-blue-600 focus:ring-blue-500"
          />
          Show trajectory
        </label>
        <button
          onClick={handleClear}
          disabled={trajectoryHistory.length === 0}
          className={`px-2 py-1 text-xs rounded transition-colors ${
            trajectoryHistory.length === 0
              ? 'text-gray-300 cursor-not-allowed'
              : 'text-gray-600 hover:bg-gray-100'
          }`}
        >
          Clear
        </button>
      </div>

      {/* Point count display */}
      {traceEnabled && trajectoryHistory.length > 0 && (
        <div className="text-xs text-gray-500">
          {trajectoryHistory.length} point{trajectoryHistory.length !== 1 ? 's' : ''}
        </div>
      )}
    </div>
  )
}
