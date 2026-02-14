import { describe, it, expect, beforeEach, vi } from 'vitest'
import React, { ReactNode, ForwardedRef } from 'react'

// Mock Konva first to prevent canvas dependency issues
vi.mock('konva', () => ({
  default: {
    Easings: {
      EaseOut: 'EaseOut',
    },
  },
}))

// Type for Konva-like props (simplified for mocking)
interface KonvaProps {
  children?: ReactNode
}

// Mock react-konva with all necessary components
// Components that receive refs use forwardRef to avoid warnings
vi.mock('react-konva', () => ({
  Stage: ({ children }: KonvaProps) => children,
  Layer: ({ children }: KonvaProps) => children,
  Rect: React.forwardRef(function MockComponent(props: KonvaProps, ref: ForwardedRef<unknown>) { void props; void ref; return null; }),
  Circle: React.forwardRef(function MockComponent(props: KonvaProps, ref: ForwardedRef<unknown>) { void props; void ref; return null; }),
  Arrow: React.forwardRef(function MockComponent(props: KonvaProps, ref: ForwardedRef<unknown>) { void props; void ref; return null; }),
  Line: React.forwardRef(function MockComponent(props: KonvaProps, ref: ForwardedRef<unknown>) { void props; void ref; return null; }),
  Image: React.forwardRef(function MockComponent(props: KonvaProps, ref: ForwardedRef<unknown>) { void props; void ref; return null; }),
  Group: ({ children }: KonvaProps) => children,
}))

// Mock the sprite hooks
vi.mock('../hooks/useSprite', () => ({
  useSprite: vi.fn(() => null), // Return null so fallback rendering is used
  useSprites: vi.fn(() => []),
}))

// Mock the ROS connection (legacy)
vi.mock('../ros/connection', () => ({
  publishMoveEntity: vi.fn(),
}))

// Mock the new ROS hooks
vi.mock('../hooks/useRosService', () => ({
  useRosPublisher: () => vi.fn(),
  useTriggerService: () => vi.fn().mockResolvedValue(true),
}))

// Mock RosConnectionProvider
vi.mock('../hooks/useRosConnection', () => ({
  useRosConnection: () => ({
    ros: {},
    isConnected: true,
    connectionError: null,
    reconnectAttempt: 0,
    maxReconnectAttempts: 10,
    retryConnection: vi.fn(),
  }),
  RosConnectionProvider: ({ children }: { children: React.ReactNode }) => children,
}))

import { render } from '@testing-library/react'
import { Canvas } from './Canvas'
import { useAppStore } from '../store/appStore'

describe('Canvas', () => {
  beforeEach(() => {
    // Reset store to clean state
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
    vi.clearAllMocks()
  })

  it('renders without crashing when no entities', () => {
    const { container } = render(<Canvas />)
    expect(container).toBeTruthy()
  })

  it('renders when robot entity exists', () => {
    useAppStore.setState({
      entities: [{ id: 'robot', type: 'robot', x: 5, y: 5, theta: 0 }],
    })
    const { container } = render(<Canvas />)
    expect(container).toBeTruthy()
  })

  it('renders object entities', () => {
    useAppStore.setState({
      entities: [
        { id: 'robot', type: 'robot', x: 5, y: 5, theta: 0 },
        { id: 'crate_1', type: 'object', x: 7, y: 3, color: 'red' },
        { id: 'crate_2', type: 'object', x: 8, y: 4, color: 'blue' },
      ],
    })
    const { container } = render(<Canvas />)
    expect(container).toBeTruthy()
  })

  it('renders wall entities', () => {
    useAppStore.setState({
      entities: [
        { id: 'robot', type: 'robot', x: 5, y: 5, theta: 0 },
        {
          id: 'wall_1',
          type: 'wall',
          x: 0,
          y: 0,
          width: 10,
          height: 0.1,
        },
      ],
    })
    const { container } = render(<Canvas />)
    expect(container).toBeTruthy()
  })

  it('renders zone entities', () => {
    useAppStore.setState({
      entities: [
        { id: 'robot', type: 'robot', x: 5, y: 5, theta: 0 },
        { id: 'zone_1', type: 'zone', x: 2, y: 2 },
      ],
    })
    const { container } = render(<Canvas />)
    expect(container).toBeTruthy()
  })

  it('renders lidar scan when ranges present', () => {
    useAppStore.setState({
      entities: [{ id: 'robot', type: 'robot', x: 5, y: 5, theta: 0 }],
      lidarRanges: [1.0, 1.5, 2.0, 2.5, 3.0],
      lidarAngleMin: -1.57,
      lidarAngleMax: 1.57,
    })
    const { container } = render(<Canvas />)
    expect(container).toBeTruthy()
  })

  it('renders robot with carrying indicator', () => {
    useAppStore.setState({
      entities: [
        {
          id: 'robot',
          type: 'robot',
          x: 5,
          y: 5,
          theta: 0,
          isCarrying: true,
        },
      ],
    })
    const { container } = render(<Canvas />)
    expect(container).toBeTruthy()
  })

  it('renders multiple entities at different positions', () => {
    useAppStore.setState({
      entities: [
        { id: 'robot', type: 'robot', x: 1, y: 1, theta: 0 },
        { id: 'obj_1', type: 'object', x: 3, y: 2, color: 'green' },
        { id: 'obj_2', type: 'object', x: 7, y: 8, color: 'yellow' },
        { id: 'zone_a', type: 'zone', x: 9, y: 1 },
        { id: 'wall_1', type: 'wall', x: 0, y: 5, width: 10, height: 0.1 },
      ],
    })
    const { container } = render(<Canvas />)
    expect(container).toBeTruthy()
  })

  it('updates when entity positions change', () => {
    const { rerender } = render(<Canvas />)
    useAppStore.setState({
      entities: [{ id: 'robot', type: 'robot', x: 5, y: 5, theta: 0 }],
    })
    rerender(<Canvas />)
    expect(useAppStore.getState().entities).toHaveLength(1)
  })

  it('handles robot at different angles', () => {
    const angles = [0, Math.PI / 4, Math.PI / 2, Math.PI, -Math.PI / 2]
    for (const theta of angles) {
      useAppStore.setState({
        entities: [{ id: 'robot', type: 'robot', x: 5, y: 5, theta }],
      })
      const { container } = render(<Canvas />)
      expect(container).toBeTruthy()
    }
  })

  it('handles empty lidar ranges', () => {
    useAppStore.setState({
      entities: [{ id: 'robot', type: 'robot', x: 5, y: 5, theta: 0 }],
      lidarRanges: [],
    })
    const { container } = render(<Canvas />)
    expect(container).toBeTruthy()
  })

  it('renders when lidar has extended range', () => {
    useAppStore.setState({
      entities: [{ id: 'robot', type: 'robot', x: 5, y: 5, theta: 0 }],
      lidarRanges: Array(360).fill(1.0) as number[],
      lidarAngleMin: -Math.PI,
      lidarAngleMax: Math.PI,
    })
    const { container } = render(<Canvas />)
    expect(container).toBeTruthy()
  })

  it('handles object with unspecified color', () => {
    useAppStore.setState({
      entities: [
        { id: 'robot', type: 'robot', x: 5, y: 5, theta: 0 },
        { id: 'obj_1', type: 'object', x: 3, y: 2 },
      ],
    })
    const { container } = render(<Canvas />)
    expect(container).toBeTruthy()
  })
})
