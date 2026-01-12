import { create } from 'zustand'

export interface Entity {
  id: string
  type: 'robot' | 'object' | 'wall' | 'zone'
  x: number
  y: number
  theta?: number
  color?: string
  width?: number
  height?: number
  isCarrying?: boolean
  carriedId?: string
}

interface AppState {
  // Connection
  connected: boolean
  setConnected: (connected: boolean) => void

  // Entities
  entities: Entity[]
  setEntities: (entities: Entity[]) => void

  // Lidar
  lidarRanges: number[]
  lidarAngleMin: number
  lidarAngleMax: number
  setLidar: (ranges: number[], angleMin: number, angleMax: number) => void

  // Task
  taskState: string
  taskIntent: string
  setTaskStatus: (state: string, intent: string) => void

  // Simulation
  simRunning: boolean
  setSimRunning: (running: boolean) => void
  simTime: number
  setSimTime: (time: number) => void

  // Selection
  selectedEntityId: string | null
  setSelectedEntityId: (id: string | null) => void
}

export const useAppStore = create<AppState>((set) => ({
  // Connection
  connected: false,
  setConnected: (connected) => set({ connected }),

  // Entities
  entities: [],
  setEntities: (entities) => set({ entities }),

  // Lidar
  lidarRanges: [],
  lidarAngleMin: -1.57,
  lidarAngleMax: 1.57,
  setLidar: (ranges, angleMin, angleMax) =>
    set({ lidarRanges: ranges, lidarAngleMin: angleMin, lidarAngleMax: angleMax }),

  // Task
  taskState: 'IDLE',
  taskIntent: '',
  setTaskStatus: (state, intent) => set({ taskState: state, taskIntent: intent }),

  // Simulation
  simRunning: false,
  setSimRunning: (running) => set({ simRunning: running }),
  simTime: 0,
  setSimTime: (time) => set({ simTime: time }),

  // Selection
  selectedEntityId: null,
  setSelectedEntityId: (id) => set({ selectedEntityId: id }),
}))
