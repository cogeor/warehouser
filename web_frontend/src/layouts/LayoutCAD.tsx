/**
 * Design 2: CAD Classic
 *
 * Top toolbar + status bar, like Fusion 360/SolidWorks.
 * Most professional CAD aesthetic.
 */

import { SimulationCanvas } from '../components/SimulationCanvas'
import { TransportControls } from '../components/ui/TransportControls'
import { StatusDisplay } from '../components/ui/StatusDisplay'
import { ConnectionDot } from '../components/ui/ConnectionDot'

export function LayoutCAD() {
  return (
    <div className="min-h-screen bg-gray-100 flex flex-col">
      {/* Top toolbar */}
      <header className="bg-white border-b border-gray-200 px-4 py-2 flex items-center justify-between">
        <div className="flex items-center gap-6">
          {/* Logo/title */}
          <div className="flex items-center gap-2">
            <div className="w-6 h-6 rounded bg-gray-900 flex items-center justify-center">
              <svg className="w-4 h-4 text-white" fill="none" stroke="currentColor" viewBox="0 0 24 24">
                <path strokeLinecap="round" strokeLinejoin="round" strokeWidth={2} d="M19 11H5m14 0a2 2 0 012 2v6a2 2 0 01-2 2H5a2 2 0 01-2-2v-6a2 2 0 012-2m14 0V9a2 2 0 00-2-2M5 11V9a2 2 0 012-2m0 0V5a2 2 0 012-2h6a2 2 0 012 2v2M7 7h10" />
              </svg>
            </div>
            <span className="font-semibold text-gray-800">Warehouser</span>
          </div>

          {/* Divider */}
          <div className="w-px h-6 bg-gray-200" />

          {/* Transport controls */}
          <TransportControls variant="labeled" />
        </div>

        {/* Right side - connection */}
        <ConnectionDot showLabel />
      </header>

      {/* Main canvas area */}
      <main className="flex-1 flex items-center justify-center p-6">
        <SimulationCanvas />
      </main>

      {/* Status bar */}
      <footer className="bg-gray-50 border-t border-gray-200 px-4 py-2">
        <StatusDisplay variant="bar" />
      </footer>
    </div>
  )
}
