import { describe, it, expect, beforeEach } from 'vitest'
import { useAppStore, MAX_TRAJECTORY_POINTS } from './appStore'

describe('AppStore', () => {
  beforeEach(() => {
    // Reset store to initial state
    useAppStore.setState({
      connected: false,
      entities: [],
      lidarRanges: [],
      lidarAngleMin: -1.57,
      lidarAngleMax: 1.57,
      lidarRobotX: 5,
      lidarRobotY: 5,
      lidarRobotTheta: 0,
      taskState: 'IDLE',
      taskIntent: '',
      simRunning: false,
      simTime: 0,
      policyEnabled: false,
      selectedEntityId: null,
      traceEnabled: false,
      trajectoryHistory: [],
    })
  })

  describe('connection', () => {
    it('sets connected state', () => {
      useAppStore.getState().setConnected(true)
      expect(useAppStore.getState().connected).toBe(true)
    })
  })

  describe('entities', () => {
    it('sets entities', () => {
      const entities = [
        { id: 'robot', type: 'robot' as const, x: 1, y: 1 },
        { id: 'obj_1', type: 'object' as const, x: 5, y: 5, color: 'red' },
      ]
      useAppStore.getState().setEntities(entities)
      expect(useAppStore.getState().entities).toHaveLength(2)
      expect(useAppStore.getState().entities[0].id).toBe('robot')
    })

    it('overwrites existing entities', () => {
      useAppStore.getState().setEntities([{ id: 'a', type: 'robot', x: 0, y: 0 }])
      useAppStore.getState().setEntities([{ id: 'b', type: 'object', x: 1, y: 1 }])
      expect(useAppStore.getState().entities).toHaveLength(1)
      expect(useAppStore.getState().entities[0].id).toBe('b')
    })
  })

  describe('lidar', () => {
    it('sets lidar data with robot pose', () => {
      const ranges = [1.0, 2.0, 3.0]
      useAppStore.getState().setLidar(ranges, -1.0, 1.0, 3.5, 4.5, 1.57)
      expect(useAppStore.getState().lidarRanges).toEqual(ranges)
      expect(useAppStore.getState().lidarAngleMin).toBe(-1.0)
      expect(useAppStore.getState().lidarAngleMax).toBe(1.0)
      expect(useAppStore.getState().lidarRobotX).toBe(3.5)
      expect(useAppStore.getState().lidarRobotY).toBe(4.5)
      expect(useAppStore.getState().lidarRobotTheta).toBe(1.57)
    })

    it('bundles robot pose with lidar scan atomically (ROS2 TF2 best practice)', () => {
      // Per ROS2 TF2 best practices, sensor data must be bundled with robot pose
      // at the time of scan to guarantee coupling. This test verifies that:
      // 1. Robot pose is stored with lidar data
      // 2. Both update together in a single atomic operation

      // First scan: robot at position (1, 1) facing east
      useAppStore.getState().setLidar([1.0, 1.5], -1.57, 1.57, 1.0, 1.0, 0)

      const state1 = useAppStore.getState()
      expect(state1.lidarRanges).toEqual([1.0, 1.5])
      expect(state1.lidarRobotX).toBe(1.0)
      expect(state1.lidarRobotY).toBe(1.0)
      expect(state1.lidarRobotTheta).toBe(0)

      // Second scan: robot moved to (2, 3) facing north
      useAppStore.getState().setLidar([2.0, 2.5], -1.57, 1.57, 2.0, 3.0, 1.57)

      const state2 = useAppStore.getState()
      expect(state2.lidarRanges).toEqual([2.0, 2.5])
      expect(state2.lidarRobotX).toBe(2.0)
      expect(state2.lidarRobotY).toBe(3.0)
      expect(state2.lidarRobotTheta).toBe(1.57)

      // Key architectural guarantee: lidar pose is independent of entity pose
      // This ensures no desync between robot sprite and lidar visualization
    })
  })

  describe('task status', () => {
    it('sets task state and intent', () => {
      useAppStore.getState().setTaskStatus('NAVIGATING_TO_PICK', 'pick')
      expect(useAppStore.getState().taskState).toBe('NAVIGATING_TO_PICK')
      expect(useAppStore.getState().taskIntent).toBe('pick')
    })
  })

  describe('simulation', () => {
    it('sets sim running state', () => {
      useAppStore.getState().setSimRunning(true)
      expect(useAppStore.getState().simRunning).toBe(true)
    })

    it('sets sim time', () => {
      useAppStore.getState().setSimTime(123.45)
      expect(useAppStore.getState().simTime).toBe(123.45)
    })
  })

  describe('selection', () => {
    it('sets selected entity id', () => {
      useAppStore.getState().setSelectedEntityId('obj_1')
      expect(useAppStore.getState().selectedEntityId).toBe('obj_1')
    })

    it('clears selection with null', () => {
      useAppStore.getState().setSelectedEntityId('obj_1')
      useAppStore.getState().setSelectedEntityId(null)
      expect(useAppStore.getState().selectedEntityId).toBeNull()
    })
  })

  describe('policy', () => {
    it('sets policy enabled state', () => {
      useAppStore.getState().setPolicyEnabled(true)
      expect(useAppStore.getState().policyEnabled).toBe(true)
    })

    it('can toggle policy enabled state', () => {
      useAppStore.getState().setPolicyEnabled(true)
      expect(useAppStore.getState().policyEnabled).toBe(true)
      useAppStore.getState().setPolicyEnabled(false)
      expect(useAppStore.getState().policyEnabled).toBe(false)
    })
  })

  describe('trajectory trace', () => {
    it('setTraceEnabled toggles state correctly', () => {
      expect(useAppStore.getState().traceEnabled).toBe(false)
      useAppStore.getState().setTraceEnabled(true)
      expect(useAppStore.getState().traceEnabled).toBe(true)
      useAppStore.getState().setTraceEnabled(false)
      expect(useAppStore.getState().traceEnabled).toBe(false)
    })

    it('addTrajectoryPoint appends points with timestamps', () => {
      const beforeTime = Date.now()
      useAppStore.getState().addTrajectoryPoint(1.5, 2.5)
      const afterTime = Date.now()

      const history = useAppStore.getState().trajectoryHistory
      expect(history).toHaveLength(1)
      expect(history[0].x).toBe(1.5)
      expect(history[0].y).toBe(2.5)
      expect(history[0].timestamp).toBeGreaterThanOrEqual(beforeTime)
      expect(history[0].timestamp).toBeLessThanOrEqual(afterTime)
    })

    it('addTrajectoryPoint accumulates multiple points', () => {
      useAppStore.getState().addTrajectoryPoint(1, 1)
      useAppStore.getState().addTrajectoryPoint(2, 2)
      useAppStore.getState().addTrajectoryPoint(3, 3)

      const history = useAppStore.getState().trajectoryHistory
      expect(history).toHaveLength(3)
      expect(history[0].x).toBe(1)
      expect(history[1].x).toBe(2)
      expect(history[2].x).toBe(3)
    })

    it('clearTrajectory empties the history', () => {
      useAppStore.getState().addTrajectoryPoint(1, 1)
      useAppStore.getState().addTrajectoryPoint(2, 2)
      expect(useAppStore.getState().trajectoryHistory).toHaveLength(2)

      useAppStore.getState().clearTrajectory()
      expect(useAppStore.getState().trajectoryHistory).toHaveLength(0)
    })

    it('enforces max points limit with circular buffer behavior', () => {
      // Add more than MAX_TRAJECTORY_POINTS
      for (let i = 0; i < MAX_TRAJECTORY_POINTS + 10; i++) {
        useAppStore.getState().addTrajectoryPoint(i, i)
      }

      const history = useAppStore.getState().trajectoryHistory
      expect(history).toHaveLength(MAX_TRAJECTORY_POINTS)
      // Oldest points should be removed, so first point should be 10
      expect(history[0].x).toBe(10)
      // Last point should be MAX_TRAJECTORY_POINTS + 9
      expect(history[history.length - 1].x).toBe(MAX_TRAJECTORY_POINTS + 9)
    })
  })
})
