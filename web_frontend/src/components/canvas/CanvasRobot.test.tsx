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
  Group: React.forwardRef(
    (
      props: { children: ReactNode; onClick?: () => void; 'data-testid'?: string },
      _ref: ForwardedRef<unknown>
    ) => (
      <div data-testid={props['data-testid'] ?? 'group'} onClick={props.onClick}>
        {props.children}
      </div>
    )
  ),
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
      data-stroke-width={props.strokeWidth}
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
  useMultipleEntityAnimations: vi.fn(() => ({
    getRef: () => () => {},
    refs: new Map(),
  })),
}))

vi.mock('../../assets/sprites', () => ({
  ROBOT_SPRITE: 'mock-robot-sprite-url',
}))

import { render, screen, fireEvent } from '@testing-library/react'
import { CanvasRobots } from './CanvasRobot'
import type { Entity } from '../../store/appStore'

describe('CanvasRobots', () => {
  const testRobot1: Entity = {
    id: 'robot1',
    type: 'robot',
    x: 5,
    y: 5,
    theta: 0,
  }

  const testRobot2: Entity = {
    id: 'robot2',
    type: 'robot',
    x: 3,
    y: 3,
    theta: Math.PI / 2,
  }

  beforeEach(() => {
    vi.clearAllMocks()
    mockSpriteReturn = null
  })

  it('renders fallback circles and arrows for multiple robots when sprite not loaded', () => {
    mockSpriteReturn = null

    render(
      <CanvasRobots robots={[testRobot1, testRobot2]} scale={60} canvasSize={600} />
    )

    // Should render circles (fallback) and arrows (direction indicator) for each robot
    const circles = screen.getAllByTestId('circle')
    const arrows = screen.getAllByTestId('arrow')
    const groups = screen.getAllByTestId('group')

    expect(groups.length).toBe(2)
    expect(circles.length).toBe(2)
    expect(arrows.length).toBe(2)
  })

  it('renders robot images for multiple robots when sprite is loaded', () => {
    mockSpriteReturn = document.createElement('img')

    render(
      <CanvasRobots robots={[testRobot1, testRobot2]} scale={60} canvasSize={600} />
    )

    const images = screen.getAllByTestId('image')
    const groups = screen.getAllByTestId('group')
    expect(groups.length).toBe(2)
    expect(images.length).toBe(2)
  })

  it('shows carrying indicator when robot isCarrying is true', () => {
    mockSpriteReturn = document.createElement('img')

    const carryingRobot: Entity = {
      ...testRobot1,
      isCarrying: true,
    }

    render(
      <CanvasRobots robots={[carryingRobot, testRobot2]} scale={60} canvasSize={600} />
    )

    // Should render carrying indicator circle for the carrying robot
    const circles = screen.getAllByTestId('circle')
    // One carrying indicator circle
    expect(circles.length).toBe(1)

    // Check the carrying indicator has orange stroke
    const carryIndicator = circles[0]
    expect(carryIndicator.getAttribute('data-stroke')).toBe('#f59e0b')
  })

  it('changes fallback fill color when robot is carrying', () => {
    mockSpriteReturn = null

    const carryingRobot: Entity = {
      ...testRobot1,
      isCarrying: true,
    }

    render(<CanvasRobots robots={[carryingRobot]} scale={60} canvasSize={600} />)

    const circles = screen.getAllByTestId('circle')
    // Fallback circle should have orange fill when carrying
    const mainCircle = circles.find((c) => c.getAttribute('data-fill') === '#f59e0b')
    expect(mainCircle).toBeTruthy()
  })

  it('highlights selected robot with different stroke', () => {
    mockSpriteReturn = null

    render(
      <CanvasRobots
        robots={[testRobot1, testRobot2]}
        selectedRobotId="robot1"
        scale={60}
        canvasSize={600}
      />
    )

    const circles = screen.getAllByTestId('circle')
    // Selected robot should have cyan stroke
    const selectedCircle = circles.find((c) => c.getAttribute('data-stroke') === '#22d3ee')
    expect(selectedCircle).toBeTruthy()
    expect(selectedCircle?.getAttribute('data-stroke-width')).toBe('4')

    // Non-selected robot should have white stroke
    const normalCircle = circles.find((c) => c.getAttribute('data-stroke') === '#fff')
    expect(normalCircle).toBeTruthy()
    expect(normalCircle?.getAttribute('data-stroke-width')).toBe('2')
  })

  it('shows selection highlight ring when robot is selected (sprite mode)', () => {
    mockSpriteReturn = document.createElement('img')

    render(
      <CanvasRobots
        robots={[testRobot1, testRobot2]}
        selectedRobotId="robot1"
        scale={60}
        canvasSize={600}
      />
    )

    // Selected robot should have cyan glow ring
    const circles = screen.getAllByTestId('circle')
    const selectionRing = circles.find((c) => c.getAttribute('data-stroke') === '#22d3ee')
    expect(selectionRing).toBeTruthy()
  })

  it('calls onRobotSelect when robot is clicked', () => {
    mockSpriteReturn = null
    const onRobotSelect = vi.fn()

    render(
      <CanvasRobots
        robots={[testRobot1, testRobot2]}
        onRobotSelect={onRobotSelect}
        scale={60}
        canvasSize={600}
      />
    )

    const groups = screen.getAllByTestId('group')
    fireEvent.click(groups[0])

    expect(onRobotSelect).toHaveBeenCalledWith('robot1')
  })

  it('renders empty when no robots provided', () => {
    mockSpriteReturn = null

    render(<CanvasRobots robots={[]} scale={60} canvasSize={600} />)

    const groups = screen.queryAllByTestId('group')
    expect(groups.length).toBe(0)
  })
})
