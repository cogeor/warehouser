import { describe, it, expect, beforeEach, vi } from 'vitest'
import React from 'react'

// Mock Konva first to prevent canvas dependency issues
vi.mock('konva', () => ({
  default: {
    Easings: {
      EaseOut: 'EaseOut',
    },
  },
}))

// Create forwardRef mocks for Konva components that receive refs
const createRefMock = (name: string) =>
  React.forwardRef((_props: any, _ref: any) => null)
createRefMock.displayName = 'MockKonvaComponent'

// Create container mocks that render children
const createContainerMock = (name: string) => {
  const Component = ({ children }: any) => children
  Component.displayName = `Mock${name}`
  return Component
}

// Mock react-konva with all necessary components
// Components that receive refs use forwardRef to avoid warnings
vi.mock('react-konva', () => ({
  Stage: ({ children }: any) => children,
  Layer: ({ children }: any) => children,
  Rect: React.forwardRef((_props: any, _ref: any) => null),
  Circle: React.forwardRef((_props: any, _ref: any) => null),
  Arrow: React.forwardRef((_props: any, _ref: any) => null),
  Line: React.forwardRef((_props: any, _ref: any) => null),
  Image: React.forwardRef((_props: any, _ref: any) => null),
  Group: ({ children }: any) => children,
}))

// Mock the sprite hooks
vi.mock('../hooks/useSprite', () => ({
  useSprite: vi.fn(() => null), // Return null so fallback rendering is used
  useSprites: vi.fn(() => []),
}))

// Mock the ROS connection
vi.mock('../ros/connection', () => ({
  publishMoveEntity: vi.fn(),
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
          x2: 10,
          y2: 0,
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
        { id: 'zone_1', type: 'zone', x: 2, y: 2, size: 2 },
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
        { id: 'zone_a', type: 'zone', x: 9, y: 1, size: 1 },
        { id: 'wall_1', type: 'wall', x: 0, y: 5, x2: 10, y2: 5 },
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
      lidarRanges: Array(360).fill(1.0),
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
