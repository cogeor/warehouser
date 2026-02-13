import { useState } from 'react'
import { publishCommand } from '../../ros/connection'
import type { PanelConfig, PanelProps } from '../../types/panels'

export const objectivePanelConfig: PanelConfig = {
  id: 'objective',
  title: 'Objective',
  defaultVisible: true,
}

export function ObjectivePanel({ isCollapsed }: PanelProps) {
  const [color, setColor] = useState('red')

  const handleSetTarget = () => {
    publishCommand(color)
  }

  return (
    <div className="bg-gray-800 rounded-lg p-4">
      <h2 className="text-lg font-semibold mb-3">Objective</h2>
      {!isCollapsed && (
        <div className="flex gap-2">
          <select
            value={color}
            onChange={(e) => setColor(e.target.value)}
            className="flex-1 bg-gray-700 text-white py-2 px-3 rounded"
          >
            <option value="red">Red</option>
            <option value="green">Green</option>
            <option value="blue">Blue</option>
          </select>
          <button
            onClick={handleSetTarget}
            className="bg-blue-600 hover:bg-blue-700 text-white py-2 px-4 rounded"
          >
            Pick
          </button>
        </div>
      )}
    </div>
  )
}
