import { memo } from 'react'
import { Canvas } from './Canvas'

interface SimulationCanvasProps {
  className?: string
}

/**
 * Centered, non-draggable simulation canvas with white theme.
 * Used by all layout designs for consistent rendering.
 */
function SimulationCanvasInner({ className = '' }: SimulationCanvasProps) {
  return (
    <div className={`flex items-center justify-center ${className}`}>
      <div className="relative">
        {/* Canvas container with white background and subtle border */}
        <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-1">
          <Canvas />
        </div>
      </div>
    </div>
  )
}

export const SimulationCanvas = memo(SimulationCanvasInner)
