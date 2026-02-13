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
  Group: ({ children }: { children: ReactNode }) => <div data-testid="group">{children}</div>,
  Line: (props: Record<string, unknown>) => (
    <div data-testid="line" data-points={JSON.stringify(props.points)} data-stroke={props.stroke} />
  ),
  Circle: (props: Record<string, unknown>) => (
    <div data-testid="circle" data-x={props.x} data-y={props.y} data-radius={props.radius} />
  ),
}))

import { render, screen } from '@testing-library/react'
import { CanvasLidar } from './CanvasLidar'

describe('CanvasLidar', () => {
  const baseProps = {
    robotX: 5,
    robotY: 5,
    robotTheta: 0,
    angleMin: -1.57,
    angleMax: 1.57,
    scale: 60,
    canvasSize: 600,
  }

  beforeEach(() => {
    vi.clearAllMocks()
  })

  it('renders nothing when ranges array is empty', () => {
    const { container } = render(<CanvasLidar {...baseProps} ranges={[]} />)

    // Should return null, so container should be empty
    expect(container.firstChild).toBeNull()
  })

  it('renders lidar rays for each range value', () => {
    const ranges = [1.0, 1.5, 2.0, 2.5, 3.0]

    render(<CanvasLidar {...baseProps} ranges={ranges} />)

    // Should render a group for each ray containing line and circle
    const groups = screen.getAllByTestId('group')
    expect(groups.length).toBe(5)
  })

  it('renders line and endpoint circle for each ray', () => {
    const ranges = [1.0, 2.0, 3.0]

    render(<CanvasLidar {...baseProps} ranges={ranges} />)

    // Should render a line for each ray
    const lines = screen.getAllByTestId('line')
    expect(lines.length).toBe(3)

    // Should render endpoint circles for each ray plus center glow
    const circles = screen.getAllByTestId('circle')
    expect(circles.length).toBe(4) // 3 endpoints + 1 center glow
  })

  it('renders center glow at robot position', () => {
    const ranges = [1.0]

    render(<CanvasLidar {...baseProps} ranges={ranges} />)

    const circles = screen.getAllByTestId('circle')
    // Last circle should be the center glow with radius 6
    const centerGlow = circles.find((c) => c.getAttribute('data-radius') === '6')
    expect(centerGlow).toBeTruthy()
  })
})
