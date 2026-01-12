import { describe, it, expect, beforeEach } from 'vitest'
import { useAppStore } from './appStore'

describe('AppStore', () => {
  beforeEach(() => {
    // Reset store to initial state
    useAppStore.setState({
      connected: false,
      entities: [],
      lidarRanges: [],
      lidarAngleMin: -1.57,
      lidarAngleMax: 1.57,
      taskState: 'IDLE',
      taskIntent: '',
      simRunning: false,
      simTime: 0,
      selectedEntityId: null,
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
    it('sets lidar data', () => {
      const ranges = [1.0, 2.0, 3.0]
      useAppStore.getState().setLidar(ranges, -1.0, 1.0)
      expect(useAppStore.getState().lidarRanges).toEqual(ranges)
      expect(useAppStore.getState().lidarAngleMin).toBe(-1.0)
      expect(useAppStore.getState().lidarAngleMax).toBe(1.0)
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
})
