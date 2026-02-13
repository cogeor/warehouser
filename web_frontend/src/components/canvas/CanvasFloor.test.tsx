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
  Image: (props: Record<string, unknown>) => <div data-testid="image" data-x={props.x} data-y={props.y} />,
  Line: (props: Record<string, unknown>) => (
    <div data-testid="line" data-points={JSON.stringify(props.points)} data-stroke={props.stroke} />
  ),
}))

// Variable to control useSprite mock return value
let mockSpriteReturn: HTMLImageElement | null = null

vi.mock('../../hooks/useSprite', () => ({
  useSprite: vi.fn(() => mockSpriteReturn),
}))

vi.mock('../../assets/sprites', () => ({
  FLOOR_TILE: 'mock-floor-tile-url',
}))

vi.mock('../../config', () => ({
  CANVAS_CONFIG: {
    CANVAS_SIZE: 600,
    WORLD_SIZE: 10,
    ANIMATION_DURATION: 100,
  },
}))

import { render, screen } from '@testing-library/react'
import { CanvasFloor } from './CanvasFloor'

describe('CanvasFloor', () => {
  beforeEach(() => {
    vi.clearAllMocks()
    mockSpriteReturn = null
  })

  it('renders fallback grid lines when sprite is not loaded', () => {
    mockSpriteReturn = null

    render(<CanvasFloor canvasSize={600} worldSize={10} tileSize={60} />)

    // Should render grid lines (both vertical and horizontal)
    const lines = screen.getAllByTestId('line')
    // 11 vertical + 11 horizontal lines for a 10m world
    expect(lines.length).toBe(22)
  })

  it('renders grid lines with correct stroke color', () => {
    mockSpriteReturn = null

    render(<CanvasFloor canvasSize={600} worldSize={10} tileSize={60} />)

    const lines = screen.getAllByTestId('line')
    // Check that lines have the correct stroke color
    lines.forEach((line) => {
      expect(line.getAttribute('data-stroke')).toBe('#333')
    })
  })

  it('renders floor tiles when sprite is loaded', () => {
    // Create a mock HTMLImageElement
    mockSpriteReturn = document.createElement('img')

    render(<CanvasFloor canvasSize={600} worldSize={10} tileSize={60} />)

    // Should render Image components for tiles
    const images = screen.getAllByTestId('image')
    // 10 tiles across x 10 tiles down = 100 tiles for 600px canvas with 60px tiles
    expect(images.length).toBeGreaterThan(0)
  })

  it('calculates correct number of tiles based on canvas and tile size', () => {
    mockSpriteReturn = document.createElement('img')

    // With 600px canvas and 100px tiles, should have 6x6 = 36 tiles
    render(<CanvasFloor canvasSize={600} worldSize={10} tileSize={100} />)

    const images = screen.getAllByTestId('image')
    expect(images.length).toBe(36)
  })
})
