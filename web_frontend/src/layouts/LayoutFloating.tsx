/**
 * Design 4: Floating Toolbar
 *
 * Floating control pills.
 * Inspired by Blender, Unity Editor.
 */

import { SimulationCanvas } from '../components/SimulationCanvas'
import { TransportControls } from '../components/ui/TransportControls'
import { StatusDisplay } from '../components/ui/StatusDisplay'

export function LayoutFloating() {
  return (
    <div className="min-h-screen bg-gray-50 relative">
      {/* Main canvas area - full screen */}
      <main className="absolute inset-0 flex items-center justify-center p-8">
        <SimulationCanvas />
      </main>

      {/* Floating transport controls - bottom left */}
      <div className="fixed bottom-6 left-6 z-40">
        <div className="bg-white rounded-lg shadow-lg border border-gray-200 px-3 py-2">
          <TransportControls variant="icon-only" />
        </div>
      </div>

      {/* Floating status - bottom right */}
      <div className="fixed bottom-6 right-6 z-40">
        <div className="bg-white rounded-lg shadow-lg border border-gray-200 px-4 py-2">
          <StatusDisplay variant="compact" />
        </div>
      </div>
    </div>
  )
}
