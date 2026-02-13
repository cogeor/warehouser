import { useState } from 'react'
import type { PanelConfig, PanelProps } from '../../types/panels'
import { Canvas } from '../Canvas'

interface LayerVisibility {
  floor: boolean
  walls: boolean
  zones: boolean
  objects: boolean
  robots: boolean
  lidar: boolean
}

export const mapPanelConfig: PanelConfig = {
  id: 'map',
  title: 'Map',
  defaultVisible: true,
  minWidth: 400,
  minHeight: 400,
}

export function MapPanel({ isCollapsed }: PanelProps) {
  const [zoom, setZoom] = useState(1)
  const [layers, setLayers] = useState<LayerVisibility>({
    floor: true,
    walls: true,
    zones: true,
    objects: true,
    robots: true,
    lidar: true,
  })

  const handleZoomIn = () => setZoom((z) => Math.min(2, z + 0.25))
  const handleZoomOut = () => setZoom((z) => Math.max(0.5, z - 0.25))
  const handleZoomReset = () => setZoom(1)

  return (
    <div className="bg-gray-800 rounded-lg overflow-hidden">
      <div className="flex justify-between items-center p-2 border-b border-gray-700">
        <h2 className="text-lg font-semibold">Map</h2>
        <div className="flex items-center gap-2">
          <span className="text-sm text-gray-400">{Math.round(zoom * 100)}%</span>
          <button
            type="button"
            onClick={handleZoomOut}
            className="px-2 py-1 bg-gray-700 hover:bg-gray-600 rounded text-sm"
            aria-label="Zoom out"
          >
            -
          </button>
          <button
            type="button"
            onClick={handleZoomReset}
            className="px-2 py-1 bg-gray-700 hover:bg-gray-600 rounded text-sm"
            aria-label="Reset zoom to 100%"
          >
            1:1
          </button>
          <button
            type="button"
            onClick={handleZoomIn}
            className="px-2 py-1 bg-gray-700 hover:bg-gray-600 rounded text-sm"
            aria-label="Zoom in"
          >
            +
          </button>
        </div>
      </div>
      <div className="flex gap-1 p-2 border-b border-gray-700 text-xs">
        {(Object.entries(layers) as [keyof LayerVisibility, boolean][]).map(([layer, visible]) => (
          <button
            key={layer}
            type="button"
            onClick={() => setLayers(l => ({ ...l, [layer]: !l[layer] }))}
            className={`px-2 py-1 rounded ${visible ? 'bg-blue-600' : 'bg-gray-700'}`}
          >
            {layer.charAt(0).toUpperCase() + layer.slice(1)}
          </button>
        ))}
      </div>
      {!isCollapsed && (
        <div className="p-2 overflow-auto">
          <div style={{ transform: `scale(${zoom})`, transformOrigin: 'top left' }}>
            <Canvas />
          </div>
        </div>
      )}
    </div>
  )
}
