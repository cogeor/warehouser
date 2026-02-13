import { describe, it, expect, beforeEach, vi } from 'vitest'
import React, { ReactNode } from 'react'

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
  Rect: (props: Record<string, unknown>) => (
    <div
      data-testid="rect"
      data-x={props.x}
      data-y={props.y}
      data-width={props.width}
      data-height={props.height}
      data-fill={props.fill}
    />
  ),
}))

vi.mock('../../hooks/useSprite', () => ({
  useSprite: vi.fn(() => null), // Return null so fallback rendering is used
}))

vi.mock('../../assets/sprites', () => ({
  WALL_TEXTURE: 'mock-wall-texture-url',
}))

import { render, screen } from '@testing-library/react'
import { CanvasWalls } from './CanvasWalls'
import type { Entity } from '../../store/appStore'

describe('CanvasWalls', () => {
  const testWalls: Entity[] = [
    { id: 'wall_1', type: 'wall', x: 0, y: 0, width: 10, height: 0.1 },
    { id: 'wall_2', type: 'wall', x: 5, y: 5, width: 0.1, height: 5 },
  ]

  beforeEach(() => {
    vi.clearAllMocks()
  })

  it('renders wall rectangles for each wall entity', () => {
    render(<CanvasWalls walls={testWalls} scale={60} canvasSize={600} />)

    const rects = screen.getAllByTestId('rect')
    expect(rects.length).toBe(2)
  })

  it('renders walls with fallback fill color when sprite not loaded', () => {
    render(<CanvasWalls walls={testWalls} scale={60} canvasSize={600} />)

    const rects = screen.getAllByTestId('rect')
    rects.forEach((rect) => {
      expect(rect.getAttribute('data-fill')).toBe('#666')
    })
  })

  it('renders nothing when walls array is empty', () => {
    render(<CanvasWalls walls={[]} scale={60} canvasSize={600} />)

    const rects = screen.queryAllByTestId('rect')
    expect(rects.length).toBe(0)
  })

  it('calculates wall dimensions correctly based on scale', () => {
    const singleWall: Entity[] = [
      { id: 'wall_1', type: 'wall', x: 0, y: 0, width: 2, height: 0.5 },
    ]

    render(<CanvasWalls walls={singleWall} scale={60} canvasSize={600} />)

    const rect = screen.getByTestId('rect')
    // Width = 2 * 60 = 120, Height = 0.5 * 60 = 30
    expect(rect.getAttribute('data-width')).toBe('120')
    expect(rect.getAttribute('data-height')).toBe('30')
  })
})
