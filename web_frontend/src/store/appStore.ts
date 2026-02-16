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

export interface TrajectoryPoint {
  x: number
  y: number
  timestamp: number
}

export const MAX_TRAJECTORY_POINTS = 1000

export interface AppState {
  // Connection
  connected: boolean
  setConnected: (connected: boolean) => void
  connectionError: string | null
  setConnectionError: (error: string | null) => void
  reconnectAttempt: number
  setReconnectAttempt: (attempt: number) => void

  // Entities
  entities: Entity[]
  setEntities: (entities: Entity[]) => void

  // Lidar (includes robot pose at time of scan - per ROS2 TF2 best practices)
  lidarRanges: number[]
  lidarAngleMin: number
  lidarAngleMax: number
  lidarRobotX: number
  lidarRobotY: number
  lidarRobotTheta: number
  setLidar: (ranges: number[], angleMin: number, angleMax: number, robotX: number, robotY: number, robotTheta: number) => void

  // Task
  taskState: string
  taskIntent: string
  setTaskStatus: (state: string, intent: string) => void

  // Simulation
  simRunning: boolean
  setSimRunning: (running: boolean) => void
  simTime: number
  setSimTime: (time: number) => void
  policyEnabled: boolean
  setPolicyEnabled: (enabled: boolean) => void

  // Selection
  selectedEntityId: string | null
  setSelectedEntityId: (id: string | null) => void

  // Demo mode
  demoActive: boolean
  setDemoActive: (active: boolean) => void

  // Multi-robot support
  selectedRobotId: string | null
  setSelectedRobotId: (id: string | null) => void

  // Trajectory trace
  traceEnabled: boolean
  setTraceEnabled: (enabled: boolean) => void
  trajectoryHistory: TrajectoryPoint[]
  addTrajectoryPoint: (x: number, y: number) => void
  clearTrajectory: () => void
}

export const useAppStore = create<AppState>((set) => ({
  // Connection
  connected: false,
  setConnected: (connected) => set({ connected }),
  connectionError: null,
  setConnectionError: (error) => set({ connectionError: error }),
  reconnectAttempt: 0,
  setReconnectAttempt: (attempt) => set({ reconnectAttempt: attempt }),

  // Entities
  entities: [],
  setEntities: (entities) => set({ entities }),

  // Lidar (includes robot pose at time of scan - per ROS2 TF2 best practices)
  lidarRanges: [],
  lidarAngleMin: -1.57,
  lidarAngleMax: 1.57,
  lidarRobotX: 5,
  lidarRobotY: 5,
  lidarRobotTheta: 0,
  setLidar: (ranges, angleMin, angleMax, robotX, robotY, robotTheta) =>
    set({ lidarRanges: ranges, lidarAngleMin: angleMin, lidarAngleMax: angleMax, lidarRobotX: robotX, lidarRobotY: robotY, lidarRobotTheta: robotTheta }),

  // Task
  taskState: 'IDLE',
  taskIntent: '',
  setTaskStatus: (state, intent) => set({ taskState: state, taskIntent: intent }),

  // Simulation
  simRunning: false,
  setSimRunning: (running) => set({ simRunning: running }),
  simTime: 0,
  setSimTime: (time) => set({ simTime: time }),
  policyEnabled: false,
  setPolicyEnabled: (enabled) => set({ policyEnabled: enabled }),

  // Selection
  selectedEntityId: null,
  setSelectedEntityId: (id) => set({ selectedEntityId: id }),

  // Demo mode
  demoActive: false,
  setDemoActive: (active) => set({ demoActive: active }),

  // Multi-robot support
  selectedRobotId: null,
  setSelectedRobotId: (id) => set({ selectedRobotId: id }),

  // Trajectory trace
  traceEnabled: false,
  setTraceEnabled: (enabled) => set({ traceEnabled: enabled }),
  trajectoryHistory: [],
  addTrajectoryPoint: (x, y) =>
    set((state) => {
      const newPoint: TrajectoryPoint = { x, y, timestamp: Date.now() }
      const newHistory = [...state.trajectoryHistory, newPoint]
      // Circular buffer behavior: remove oldest when full
      if (newHistory.length > MAX_TRAJECTORY_POINTS) {
        return { trajectoryHistory: newHistory.slice(newHistory.length - MAX_TRAJECTORY_POINTS) }
      }
      return { trajectoryHistory: newHistory }
    }),
  clearTrajectory: () => set({ trajectoryHistory: [] }),
}))

export function selectRobots(state: AppState): Entity[] {
  return state.entities.filter(e => e.type === 'robot')
}

export function selectSelectedRobot(state: AppState): Entity | undefined {
  const robots = selectRobots(state)
  if (state.selectedRobotId) {
    return robots.find(r => r.id === state.selectedRobotId)
  }
  return robots[0] // Default to first robot
}
