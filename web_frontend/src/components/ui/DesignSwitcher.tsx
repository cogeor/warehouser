import { useEffect } from 'react'
import { useDesignStore, DESIGNS, DesignId } from '../../store/designStore'

export function DesignSwitcher() {
  const currentDesign = useDesignStore((s) => s.currentDesign)
  const setDesign = useDesignStore((s) => s.setDesign)
  const nextDesign = useDesignStore((s) => s.nextDesign)
  const prevDesign = useDesignStore((s) => s.prevDesign)

  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      // Ignore if typing in an input
      if (e.target instanceof HTMLInputElement || e.target instanceof HTMLTextAreaElement) {
        return
      }

      // Number keys 1-5 to select design
      if (e.key >= '1' && e.key <= '5') {
        setDesign(parseInt(e.key) as DesignId)
        return
      }

      // Arrow keys to cycle
      if (e.key === '[' || e.key === 'ArrowLeft' && e.altKey) {
        prevDesign()
        return
      }
      if (e.key === ']' || e.key === 'ArrowRight' && e.altKey) {
        nextDesign()
        return
      }
    }

    window.addEventListener('keydown', handleKeyDown)
    return () => window.removeEventListener('keydown', handleKeyDown)
  }, [setDesign, nextDesign, prevDesign])

  return (
    <div className="fixed bottom-4 left-4 z-50">
      <div className="bg-white rounded-lg shadow-lg border border-gray-200 p-3">
        <div className="text-xs text-gray-500 mb-2">Press 1-5 to switch design</div>
        <div className="flex gap-1">
          {([1, 2, 3, 4, 5] as DesignId[]).map((id) => (
            <button
              key={id}
              onClick={() => setDesign(id)}
              className={`w-8 h-8 rounded text-sm font-medium transition-colors ${
                currentDesign === id
                  ? 'bg-gray-900 text-white'
                  : 'bg-gray-100 text-gray-600 hover:bg-gray-200'
              }`}
              title={DESIGNS[id].name}
            >
              {id}
            </button>
          ))}
        </div>
        <div className="mt-2 text-xs">
          <div className="font-medium text-gray-700">{DESIGNS[currentDesign].name}</div>
          <div className="text-gray-500">{DESIGNS[currentDesign].description}</div>
        </div>
      </div>
    </div>
  )
}
