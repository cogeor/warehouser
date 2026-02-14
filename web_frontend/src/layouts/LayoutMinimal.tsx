/**
 * Design 1: Minimal Studio
 *
 * Ultra-minimal with floating controls at bottom center.
 * Inspired by Figma, Linear, modern design tools.
 */

import { SimulationCanvas } from '../components/SimulationCanvas'
import { TransportControls } from '../components/ui/TransportControls'
import { StatusDisplay } from '../components/ui/StatusDisplay'

export function LayoutMinimal() {
  return (
    <div className="min-h-screen bg-gray-50 flex flex-col">
      {/* Main canvas area - centered */}
      <main className="flex-1 flex items-center justify-center p-8">
        <SimulationCanvas />
      </main>

      {/* Floating controls at bottom center */}
      <div className="fixed bottom-8 left-1/2 -translate-x-1/2">
        <div className="bg-white rounded-full shadow-lg border border-gray-200 px-4 py-2 flex items-center gap-4">
          <TransportControls variant="minimal" />
          <div className="w-px h-6 bg-gray-200" />
          <StatusDisplay variant="compact" />
        </div>
      </div>
    </div>
  )
}
