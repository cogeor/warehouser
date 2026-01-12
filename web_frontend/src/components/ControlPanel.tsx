import { callService } from '../ros/connection'
import { useAppStore } from '../store/appStore'

export function ControlPanel() {
  const simRunning = useAppStore((s) => s.simRunning)
  const setSimRunning = useAppStore((s) => s.setSimRunning)

  const handleStart = async () => {
    const success = await callService('/sim/start')
    if (success) setSimRunning(true)
  }

  const handlePause = async () => {
    const success = await callService('/sim/pause')
    if (success) setSimRunning(false)
  }

  const handleReset = async () => {
    await callService('/sim/reset')
    setSimRunning(false)
  }

  return (
    <div className="bg-gray-800 rounded-lg p-4">
      <h2 className="text-lg font-semibold mb-3">Controls</h2>
      <div className="flex gap-2">
        {!simRunning ? (
          <button
            onClick={handleStart}
            className="flex-1 bg-green-600 hover:bg-green-700 text-white py-2 px-4 rounded"
          >
            Start
          </button>
        ) : (
          <button
            onClick={handlePause}
            className="flex-1 bg-yellow-600 hover:bg-yellow-700 text-white py-2 px-4 rounded"
          >
            Pause
          </button>
        )}
        <button
          onClick={handleReset}
          className="flex-1 bg-red-600 hover:bg-red-700 text-white py-2 px-4 rounded"
        >
          Reset
        </button>
      </div>
    </div>
  )
}
