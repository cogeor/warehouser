import { describe, it, expect, beforeEach, vi } from 'vitest'
import type { ReactNode } from 'react'

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
  Image: (props: Record<string, unknown>) => (
    <div data-testid="image" data-x={props.x} data-y={props.y} />
  ),
  Circle: (props: Record<string, unknown>) => (
    <div
      data-testid="circle"
      data-x={props.x}
      data-y={props.y}
      data-radius={props.radius}
      data-fill={props.fill}
    />
  ),
}))

vi.mock('../../hooks/useSprite', () => ({
  useSprite: vi.fn(() => null), // Return null so fallback rendering is used
}))

vi.mock('../../assets/sprites', () => ({
  ZONE_MARKER: 'mock-zone-marker-url',
}))

import { render, screen } from '@testing-library/react'
import { CanvasZones } from './CanvasZones'
import type { Entity } from '../../store/appStore'

describe('CanvasZones', () => {
  const testZones: Entity[] = [
    { id: 'zone_1', type: 'zone', x: 2, y: 2 },
    { id: 'zone_2', type: 'zone', x: 8, y: 8 },
    { id: 'zone_3', type: 'zone', x: 5, y: 5 },
  ]

  beforeEach(() => {
    vi.clearAllMocks()
  })

  it('renders circle for each zone entity when sprite not loaded', () => {
    render(<CanvasZones zones={testZones} scale={60} canvasSize={600} />)

    const circles = screen.getAllByTestId('circle')
    expect(circles.length).toBe(3)
  })

  it('renders zones with correct fallback fill color', () => {
    render(<CanvasZones zones={testZones} scale={60} canvasSize={600} />)

    const circles = screen.getAllByTestId('circle')
    circles.forEach((circle) => {
      expect(circle.getAttribute('data-fill')).toBe('rgba(100, 200, 100, 0.3)')
    })
  })

  it('renders nothing when zones array is empty', () => {
    render(<CanvasZones zones={[]} scale={60} canvasSize={600} />)

    const circles = screen.queryAllByTestId('circle')
    expect(circles.length).toBe(0)
  })

  it('calculates zone radius correctly based on scale', () => {
    const singleZone: Entity[] = [{ id: 'zone_1', type: 'zone', x: 5, y: 5 }]

    render(<CanvasZones zones={singleZone} scale={60} canvasSize={600} />)

    const circle = screen.getByTestId('circle')
    // Default zone radius is 0.5 meters, so 0.5 * 60 = 30 pixels
    expect(circle.getAttribute('data-radius')).toBe('30')
  })
})
