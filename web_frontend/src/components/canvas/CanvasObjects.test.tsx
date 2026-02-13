import { describe, it, expect, beforeEach, vi } from 'vitest'
import React, { ReactNode, ForwardedRef } from 'react'

// Mock Konva first
vi.mock('konva', () => ({
  default: {
    Easings: { EaseOut: 'EaseOut' },
  },
}))

// Store drag end handler for testing
let lastDragEndHandler: ((e: { target: { x: () => number; y: () => number } }) => void) | null = null

// Mock react-konva components
vi.mock('react-konva', () => ({
  Stage: ({ children }: { children: ReactNode }) => <div data-testid="stage">{children}</div>,
  Layer: ({ children }: { children: ReactNode }) => <div data-testid="layer">{children}</div>,
  Image: React.forwardRef((props: Record<string, unknown>, _ref: ForwardedRef<unknown>) => {
    if (props.onDragEnd) {
      lastDragEndHandler = props.onDragEnd as (e: { target: { x: () => number; y: () => number } }) => void
    }
    return (
      <div
        data-testid="image"
        data-x={props.x}
        data-y={props.y}
        data-draggable={props.draggable}
      />
    )
  }),
  Circle: React.forwardRef((props: Record<string, unknown>, _ref: ForwardedRef<unknown>) => {
    if (props.onDragEnd) {
      lastDragEndHandler = props.onDragEnd as (e: { target: { x: () => number; y: () => number } }) => void
    }
    return (
      <div
        data-testid="circle"
        data-x={props.x}
        data-y={props.y}
        data-fill={props.fill}
        data-draggable={props.draggable}
      />
    )
  }),
}))

vi.mock('../../hooks/useSprite', () => ({
  useSprites: vi.fn(() => ({})), // Return empty object so fallback rendering is used
}))

vi.mock('../../hooks/useEntityAnimation', () => ({
  useMultipleEntityAnimations: vi.fn(() => ({
    getRef: () => () => {},
    refs: new Map(),
  })),
}))

vi.mock('../../assets/sprites/index', () => ({
  CRATE_SPRITES: {},
}))

import { render, screen } from '@testing-library/react'
import { CanvasObjects } from './CanvasObjects'
import type { Entity } from '../../store/appStore'

describe('CanvasObjects', () => {
  const testObjects: Entity[] = [
    { id: 'obj_1', type: 'object', x: 3, y: 2, color: 'red' },
    { id: 'obj_2', type: 'object', x: 7, y: 8, color: 'blue' },
  ]

  beforeEach(() => {
    vi.clearAllMocks()
    lastDragEndHandler = null
  })

  it('renders circle for each object when sprites not loaded', () => {
    render(<CanvasObjects objects={testObjects} scale={60} canvasSize={600} />)

    const circles = screen.getAllByTestId('circle')
    expect(circles.length).toBe(2)
  })

  it('renders objects as draggable', () => {
    render(<CanvasObjects objects={testObjects} scale={60} canvasSize={600} />)

    const circles = screen.getAllByTestId('circle')
    circles.forEach((circle) => {
      expect(circle.getAttribute('data-draggable')).toBe('true')
    })
  })

  it('renders nothing when objects array is empty', () => {
    render(<CanvasObjects objects={[]} scale={60} canvasSize={600} />)

    const circles = screen.queryAllByTestId('circle')
    expect(circles.length).toBe(0)
  })

  it('calls onObjectMoved callback when object is dragged', () => {
    const onObjectMoved = vi.fn()

    render(
      <CanvasObjects
        objects={[{ id: 'obj_1', type: 'object', x: 3, y: 2, color: 'red' }]}
        scale={60}
        canvasSize={600}
        onObjectMoved={onObjectMoved}
      />
    )

    // Simulate drag end event
    if (lastDragEndHandler) {
      lastDragEndHandler({
        target: {
          x: () => 300,
          y: () => 300,
        },
      })
    }

    expect(onObjectMoved).toHaveBeenCalled()
    // The callback receives world coordinates converted from canvas coordinates
    expect(onObjectMoved).toHaveBeenCalledWith('obj_1', expect.any(Number), expect.any(Number))
  })
})
