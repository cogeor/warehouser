/**
 * Design 5: Immersive
 *
 * Full-screen canvas, minimal chrome.
 * Maximum focus on simulation.
 */

import { useState } from 'react'
import { SimulationCanvas } from '../components/SimulationCanvas'
import { TransportControls } from '../components/ui/TransportControls'
import { StatusDisplay } from '../components/ui/StatusDisplay'

export function LayoutImmersive() {
  const [showControls, setShowControls] = useState(false)

  return (
    <div
      className="min-h-screen bg-white relative"
      onMouseEnter={() => setShowControls(true)}
      onMouseLeave={() => setShowControls(false)}
    >
      {/* Main canvas area - centered on white */}
      <main className="absolute inset-0 flex items-center justify-center">
        <SimulationCanvas />
      </main>

      {/* Controls that appear on hover - centered bottom */}
      <div
        className={`fixed bottom-8 left-1/2 -translate-x-1/2 z-40 transition-all duration-200 ${
          showControls ? 'opacity-100 translate-y-0' : 'opacity-0 translate-y-4 pointer-events-none'
        }`}
      >
        <div className="bg-white/90 backdrop-blur-sm rounded-xl shadow-lg border border-gray-200 px-5 py-3">
          <div className="flex items-center gap-6">
            <TransportControls variant="icon-only" />
            <div className="w-px h-6 bg-gray-200" />
            <StatusDisplay variant="compact" />
          </div>
        </div>
      </div>

      {/* Subtle hint to hover */}
      <div
        className={`fixed bottom-4 left-1/2 -translate-x-1/2 text-xs text-gray-400 transition-opacity duration-200 ${
          showControls ? 'opacity-0' : 'opacity-100'
        }`}
      >
        Hover to show controls
      </div>
    </div>
  )
}
