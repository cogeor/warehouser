import { create } from 'zustand'
import { persist } from 'zustand/middleware'

export type DesignId = 1 | 2 | 3 | 4 | 5

export interface DesignInfo {
  id: DesignId
  name: string
  description: string
}

export const DESIGNS: Record<DesignId, DesignInfo> = {
  1: {
    id: 1,
    name: 'Minimal Studio',
    description: 'Ultra-minimal with floating controls. Modern design tool feel.',
  },
  2: {
    id: 2,
    name: 'CAD Classic',
    description: 'Top toolbar + status bar. Professional CAD aesthetic.',
  },
  3: {
    id: 3,
    name: 'Split View',
    description: 'Right sidebar with controls/status. Traditional simulation layout.',
  },
  4: {
    id: 4,
    name: 'Floating Toolbar',
    description: 'Floating control pills. Blender/Unity inspired.',
  },
  5: {
    id: 5,
    name: 'Immersive',
    description: 'Full-screen canvas, minimal chrome. Maximum focus.',
  },
}

interface DesignState {
  currentDesign: DesignId
  setDesign: (id: DesignId) => void
  nextDesign: () => void
  prevDesign: () => void
}

export const useDesignStore = create<DesignState>()(
  persist(
    (set, get) => ({
      currentDesign: 2, // Default to CAD Classic
      setDesign: (id) => set({ currentDesign: id }),
      nextDesign: () => {
        const current = get().currentDesign
        const next = current === 5 ? 1 : ((current + 1) as DesignId)
        set({ currentDesign: next })
      },
      prevDesign: () => {
        const current = get().currentDesign
        const prev = current === 1 ? 5 : ((current - 1) as DesignId)
        set({ currentDesign: prev })
      },
    }),
    {
      name: 'warehouser-design',
    }
  )
)
