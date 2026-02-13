import { describe, it, expect, beforeEach, vi } from 'vitest'
import React, { ReactNode, ForwardedRef } from 'react'

// Mock Konva first
vi.mock('konva', () => ({
  default: {
    Easings: { EaseOut: 'EaseOut' },
  },
}))

// Mock react-konva components
vi.mock('react-konva', () => ({
  Stage: ({ children }: { children: ReactNode }) => <div data-testid="stage">{children}</div>,
  Layer: ({ children }: { children: ReactNode }) => <div data-testid="layer">{children}</div>,
  Image: React.forwardRef((props: Record<string, unknown>, _ref: ForwardedRef<unknown>) => (
    <div data-testid="image" data-x={props.x} data-y={props.y} data-rotation={props.rotation} />
  )),
  Circle: React.forwardRef((props: Record<string, unknown>, _ref: ForwardedRef<unknown>) => (
    <div
      data-testid="circle"
      data-x={props.x}
      data-y={props.y}
      data-radius={props.radius}
      data-fill={props.fill}
      data-stroke={props.stroke}
    />
  )),
  Arrow: React.forwardRef((props: Record<string, unknown>, _ref: ForwardedRef<unknown>) => (
    <div data-testid="arrow" data-x={props.x} data-y={props.y} />
  )),
}))

// Variable to control useSprite mock return value
let mockSpriteReturn: HTMLImageElement | null = null

vi.mock('../../hooks/useSprite', () => ({
  useSprite: vi.fn(() => mockSpriteReturn),
}))

vi.mock('../../hooks/useEntityAnimation', () => ({
  useEntityAnimation: vi.fn(() => ({ current: null })),
}))

vi.mock('../../assets/sprites', () => ({
  ROBOT_SPRITE: 'mock-robot-sprite-url',
}))

import { render, screen } from '@testing-library/react'
import { CanvasRobot } from './CanvasRobot'
import type { Entity } from '../../store/appStore'

describe('CanvasRobot', () => {
  const testRobot: Entity = {
    id: 'robot',
    type: 'robot',
    x: 5,
    y: 5,
    theta: 0,
  }

  beforeEach(() => {
    vi.clearAllMocks()
    mockSpriteReturn = null
  })

  it('renders fallback circle and arrow when sprite not loaded', () => {
    mockSpriteReturn = null

    render(<CanvasRobot robot={testRobot} scale={60} canvasSize={600} />)

    // Should render circle (fallback) and arrow (direction indicator)
    const circles = screen.getAllByTestId('circle')
    const arrows = screen.getAllByTestId('arrow')

    expect(circles.length).toBeGreaterThan(0)
    expect(arrows.length).toBe(1)
  })

  it('renders robot image when sprite is loaded', () => {
    mockSpriteReturn = document.createElement('img')

    render(<CanvasRobot robot={testRobot} scale={60} canvasSize={600} />)

    const images = screen.getAllByTestId('image')
    expect(images.length).toBe(1)
  })

  it('shows carrying indicator when robot isCarrying is true', () => {
    mockSpriteReturn = document.createElement('img')

    const carryingRobot: Entity = {
      ...testRobot,
      isCarrying: true,
    }

    render(<CanvasRobot robot={carryingRobot} scale={60} canvasSize={600} />)

    // Should render image plus carrying indicator circle
    const circles = screen.getAllByTestId('circle')
    expect(circles.length).toBe(1) // Carrying indicator ring

    // Check the carrying indicator has orange stroke
    const carryIndicator = circles[0]
    expect(carryIndicator.getAttribute('data-stroke')).toBe('#f59e0b')
  })

  it('changes fallback fill color when robot is carrying', () => {
    mockSpriteReturn = null

    const carryingRobot: Entity = {
      ...testRobot,
      isCarrying: true,
    }

    render(<CanvasRobot robot={carryingRobot} scale={60} canvasSize={600} />)

    const circles = screen.getAllByTestId('circle')
    // Fallback circle should have orange fill when carrying
    const mainCircle = circles.find((c) => c.getAttribute('data-fill') === '#f59e0b')
    expect(mainCircle).toBeTruthy()
  })
})
