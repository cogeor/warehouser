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
  Line: (props: Record<string, unknown>) => (
    <div
      data-testid="line"
      data-points={JSON.stringify(props.points)}
      data-stroke={props.stroke}
      data-strokewidth={props.strokeWidth}
      data-opacity={props.opacity}
      data-linecap={props.lineCap}
      data-linejoin={props.lineJoin}
    />
  ),
}))

import { render, screen } from '@testing-library/react'
import { CanvasTrajectory } from './CanvasTrajectory'
import type { TrajectoryPoint } from '../../store/appStore'

describe('CanvasTrajectory', () => {
  const baseProps = {
    scale: 60,
    canvasSize: 600,
  }

  beforeEach(() => {
    vi.clearAllMocks()
  })

  it('returns null with fewer than 2 points', () => {
    const points: TrajectoryPoint[] = [{ x: 1, y: 1, timestamp: 1000 }]
    const { container } = render(<CanvasTrajectory {...baseProps} points={points} />)
    expect(container.firstChild).toBeNull()
  })

  it('returns null with empty points array', () => {
    const { container } = render(<CanvasTrajectory {...baseProps} points={[]} />)
    expect(container.firstChild).toBeNull()
  })

  it('renders Line with correct points array', () => {
    const points: TrajectoryPoint[] = [
      { x: 1, y: 1, timestamp: 1000 },
      { x: 2, y: 2, timestamp: 2000 },
      { x: 3, y: 3, timestamp: 3000 },
    ]

    render(<CanvasTrajectory {...baseProps} points={points} />)

    const line = screen.getByTestId('line')
    expect(line).toBeTruthy()

    // Check that points are flattened to [x1, y1, x2, y2, ...]
    const renderedPoints = JSON.parse(line.getAttribute('data-points') || '[]')
    expect(renderedPoints.length).toBe(6) // 3 points * 2 coordinates
  })

  it('applies coordinate transformation correctly', () => {
    // With scale=60 and canvasSize=600, worldSize=10
    // World (0, 10) should map to Canvas (0, 0) - top-left
    // World (0, 0) should map to Canvas (0, 600) - bottom-left
    const points: TrajectoryPoint[] = [
      { x: 0, y: 0, timestamp: 1000 },
      { x: 10, y: 10, timestamp: 2000 },
    ]

    render(<CanvasTrajectory {...baseProps} points={points} />)

    const line = screen.getByTestId('line')
    const renderedPoints = JSON.parse(line.getAttribute('data-points') || '[]')

    // First point: world (0, 0) -> canvas (0, 600)
    expect(renderedPoints[0]).toBe(0) // x
    expect(renderedPoints[1]).toBe(600) // y (flipped)

    // Second point: world (10, 10) -> canvas (600, 0)
    expect(renderedPoints[2]).toBe(600) // x
    expect(renderedPoints[3]).toBe(0) // y (flipped)
  })

  it('uses default styling values', () => {
    const points: TrajectoryPoint[] = [
      { x: 1, y: 1, timestamp: 1000 },
      { x: 2, y: 2, timestamp: 2000 },
    ]

    render(<CanvasTrajectory {...baseProps} points={points} />)

    const line = screen.getByTestId('line')
    expect(line.getAttribute('data-stroke')).toBe('#3b82f6') // Default blue
    expect(line.getAttribute('data-strokewidth')).toBe('2')
    expect(line.getAttribute('data-opacity')).toBe('0.6')
  })

  it('applies custom styling props', () => {
    const points: TrajectoryPoint[] = [
      { x: 1, y: 1, timestamp: 1000 },
      { x: 2, y: 2, timestamp: 2000 },
    ]

    render(
      <CanvasTrajectory
        {...baseProps}
        points={points}
        color="#ff0000"
        strokeWidth={4}
        opacity={0.8}
      />
    )

    const line = screen.getByTestId('line')
    expect(line.getAttribute('data-stroke')).toBe('#ff0000')
    expect(line.getAttribute('data-strokewidth')).toBe('4')
    expect(line.getAttribute('data-opacity')).toBe('0.8')
  })

  it('applies rounded line caps and joins', () => {
    const points: TrajectoryPoint[] = [
      { x: 1, y: 1, timestamp: 1000 },
      { x: 2, y: 2, timestamp: 2000 },
    ]

    render(<CanvasTrajectory {...baseProps} points={points} />)

    const line = screen.getByTestId('line')
    expect(line.getAttribute('data-linecap')).toBe('round')
    expect(line.getAttribute('data-linejoin')).toBe('round')
  })
})
