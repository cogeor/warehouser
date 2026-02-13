import type { PanelConfig, PanelProps } from '../../types/panels'
import { Canvas } from '../Canvas'

export const mapPanelConfig: PanelConfig = {
  id: 'map',
  title: 'Map',
  defaultVisible: true,
  minWidth: 400,
  minHeight: 400,
}

export function MapPanel({ isCollapsed }: PanelProps) {
  return (
    <div className="bg-gray-800 rounded-lg overflow-hidden">
      <div className="flex justify-between items-center p-2 border-b border-gray-700">
        <h2 className="text-lg font-semibold">Map</h2>
        <div className="flex gap-2">
          {/* Future: zoom controls, layer toggles */}
        </div>
      </div>
      {!isCollapsed && (
        <div className="p-2">
          <Canvas />
        </div>
      )}
    </div>
  )
}
