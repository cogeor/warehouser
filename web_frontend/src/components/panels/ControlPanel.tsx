import { useRef } from 'react'
import { callService, publishCommand } from '../../ros/connection'
import { useAppStore } from '../../store/appStore'
import type { PanelConfig, PanelProps } from '../../types/panels'

const DEMO_COLORS = ['red', 'green', 'blue', 'yellow'] as const
const DEMO_INTERVAL_MS = 5000

export const controlPanelConfig: PanelConfig = {
  id: 'controls',
  title: 'Controls',
  defaultVisible: true,
}

export function ControlPanel({ isCollapsed }: PanelProps) {
  const simRunning = useAppStore((s) => s.simRunning)
  const setSimRunning = useAppStore((s) => s.setSimRunning)
  const demoActive = useAppStore((s) => s.demoActive)
  const setDemoActive = useAppStore((s) => s.setDemoActive)

  const demoIntervalRef = useRef<number | null>(null)
  const colorIndexRef = useRef(0)

  const handleStart = async () => {
    const success = await callService('/sim/start')
    if (success) setSimRunning(true)
  }

  const handlePause = async () => {
    const success = await callService('/sim/pause')
    if (success) setSimRunning(false)
  }

  const handleReset = async () => {
    // Stop demo if running
    if (demoActive) {
      stopAutoDemo()
    }
    await callService('/sim/reset')
    setSimRunning(false)
  }

  const startAutoDemo = async () => {
    setDemoActive(true)
    colorIndexRef.current = 0

    // Start simulation if not running
    if (!simRunning) {
      const success = await callService('/sim/start')
      if (success) setSimRunning(true)
    }

    // Send first command immediately
    publishCommand(DEMO_COLORS[colorIndexRef.current])
    colorIndexRef.current = (colorIndexRef.current + 1) % DEMO_COLORS.length

    // Cycle through commands
    demoIntervalRef.current = window.setInterval(() => {
      publishCommand(DEMO_COLORS[colorIndexRef.current])
      colorIndexRef.current = (colorIndexRef.current + 1) % DEMO_COLORS.length
    }, DEMO_INTERVAL_MS)
  }

  const stopAutoDemo = () => {
    setDemoActive(false)
    if (demoIntervalRef.current !== null) {
      clearInterval(demoIntervalRef.current)
      demoIntervalRef.current = null
    }
  }

  const handleDemoToggle = () => {
    if (demoActive) {
      stopAutoDemo()
    } else {
      startAutoDemo()
    }
  }

  return (
    <div className="bg-gray-800 rounded-lg p-4">
      <h2 className="text-lg font-semibold mb-3">Controls</h2>

      {!isCollapsed && (
        <>
          {/* Simulation controls */}
          <div className="flex gap-2 mb-3">
            {!simRunning ? (
              <button
                onClick={handleStart}
                disabled={demoActive}
                className={`flex-1 py-2 px-4 rounded text-white ${
                  demoActive
                    ? 'bg-gray-600 cursor-not-allowed'
                    : 'bg-green-600 hover:bg-green-700'
                }`}
              >
                Start
              </button>
            ) : (
              <button
                onClick={handlePause}
                disabled={demoActive}
                className={`flex-1 py-2 px-4 rounded text-white ${
                  demoActive
                    ? 'bg-gray-600 cursor-not-allowed'
                    : 'bg-yellow-600 hover:bg-yellow-700'
                }`}
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

          {/* Auto Demo button */}
          <div className="border-t border-gray-700 pt-3">
            <button
              onClick={handleDemoToggle}
              className={`w-full py-3 px-4 rounded font-medium text-white transition-all ${
                demoActive
                  ? 'bg-orange-600 hover:bg-orange-700 animate-pulse'
                  : 'bg-purple-600 hover:bg-purple-700'
              }`}
            >
              {demoActive ? (
                <span className="flex items-center justify-center gap-2">
                  <span className="w-2 h-2 bg-white rounded-full animate-ping" />
                  Stop Demo
                </span>
              ) : (
                <span className="flex items-center justify-center gap-2">
                  <svg
                    className="w-5 h-5"
                    fill="none"
                    stroke="currentColor"
                    viewBox="0 0 24 24"
                  >
                    <path
                      strokeLinecap="round"
                      strokeLinejoin="round"
                      strokeWidth={2}
                      d="M14.752 11.168l-3.197-2.132A1 1 0 0010 9.87v4.263a1 1 0 001.555.832l3.197-2.132a1 1 0 000-1.664z"
                    />
                    <path
                      strokeLinecap="round"
                      strokeLinejoin="round"
                      strokeWidth={2}
                      d="M21 12a9 9 0 11-18 0 9 9 0 0118 0z"
                    />
                  </svg>
                  Auto Demo
                </span>
              )}
            </button>
            {demoActive && (
              <p className="text-sm text-gray-400 text-center mt-2">
                Demo running... cycling through colors
              </p>
            )}
          </div>
        </>
      )}
    </div>
  )
}
