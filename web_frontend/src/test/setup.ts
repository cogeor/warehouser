import '@testing-library/jest-dom'
import { vi } from 'vitest'

// Mock canvas for Konva
HTMLCanvasElement.prototype.getContext = vi.fn(() => ({
  fillRect: vi.fn(),
  clearRect: vi.fn(),
  getImageData: vi.fn(() => ({ data: [] })),
  putImageData: vi.fn(),
  createImageData: vi.fn(() => []),
  setTransform: vi.fn(),
  drawImage: vi.fn(),
  save: vi.fn(),
  restore: vi.fn(),
  beginPath: vi.fn(),
  moveTo: vi.fn(),
  lineTo: vi.fn(),
  closePath: vi.fn(),
  stroke: vi.fn(),
  fill: vi.fn(),
  translate: vi.fn(),
  scale: vi.fn(),
  rotate: vi.fn(),
  arc: vi.fn(),
  measureText: vi.fn(() => ({ width: 0 })),
  transform: vi.fn(),
  rect: vi.fn(),
  clip: vi.fn(),
})) as unknown as typeof HTMLCanvasElement.prototype.getContext

// Mock ROSLIB
vi.mock('roslib', () => ({
  default: {
    Ros: vi.fn().mockImplementation(() => ({
      on: vi.fn(),
      connect: vi.fn(),
      close: vi.fn(),
    })),
    Topic: vi.fn().mockImplementation(() => ({
      subscribe: vi.fn(),
      publish: vi.fn(),
      unsubscribe: vi.fn(),
    })),
    Service: vi.fn().mockImplementation(() => ({
      callService: vi.fn((_, callback) => callback({ success: true })),
    })),
    ServiceRequest: vi.fn(),
    Message: vi.fn((data) => data),
  },
}))
