import { describe, it, expect } from 'vitest'
import {
  CoordinateTransform,
  Transform2D,
  WORLD_SIZE,
  CANVAS_SIZE,
  SCALE,
  defaultTransform,
} from './transforms'

describe('CoordinateTransform', () => {
  describe('constants', () => {
    it('has correct default values', () => {
      expect(WORLD_SIZE).toBe(10)
      expect(CANVAS_SIZE).toBe(600)
      expect(SCALE).toBe(60)
    })
  })

  describe('constructor', () => {
    it('creates instance with default values', () => {
      const transform = new CoordinateTransform()
      expect(transform.worldSize).toBe(WORLD_SIZE)
      expect(transform.canvasSize).toBe(CANVAS_SIZE)
      expect(transform.scale).toBe(SCALE)
    })

    it('creates instance with custom values', () => {
      const transform = new CoordinateTransform(20, 800)
      expect(transform.worldSize).toBe(20)
      expect(transform.canvasSize).toBe(800)
      expect(transform.scale).toBe(40)
    })

    it('throws error for non-positive worldSize', () => {
      expect(() => new CoordinateTransform(0, 600)).toThrow('worldSize must be positive')
      expect(() => new CoordinateTransform(-10, 600)).toThrow('worldSize must be positive')
    })

    it('throws error for non-positive canvasSize', () => {
      expect(() => new CoordinateTransform(10, 0)).toThrow('canvasSize must be positive')
      expect(() => new CoordinateTransform(10, -600)).toThrow('canvasSize must be positive')
    })
  })

  describe('worldToCanvas', () => {
    const transform = new CoordinateTransform()

    it('converts origin correctly', () => {
      const [cx, cy] = transform.worldToCanvas(0, 0)
      expect(cx).toBe(0)
      expect(cy).toBe(600) // Bottom of canvas
    })

    it('converts top-right world corner to top-right canvas', () => {
      const [cx, cy] = transform.worldToCanvas(10, 10)
      expect(cx).toBe(600)
      expect(cy).toBe(0) // Top of canvas
    })

    it('converts center correctly', () => {
      const [cx, cy] = transform.worldToCanvas(5, 5)
      expect(cx).toBe(300)
      expect(cy).toBe(300)
    })

    it('handles fractional coordinates', () => {
      const [cx, cy] = transform.worldToCanvas(2.5, 7.5)
      expect(cx).toBe(150)
      expect(cy).toBe(150)
    })

    it('handles negative coordinates (outside world bounds)', () => {
      const [cx, cy] = transform.worldToCanvas(-1, -1)
      expect(cx).toBe(-60)
      expect(cy).toBe(660)
    })

    it('handles coordinates beyond world size', () => {
      const [cx, cy] = transform.worldToCanvas(15, 15)
      expect(cx).toBe(900)
      expect(cy).toBe(-300)
    })
  })

  describe('canvasToWorld', () => {
    const transform = new CoordinateTransform()

    it('converts canvas origin (top-left) correctly', () => {
      const [wx, wy] = transform.canvasToWorld(0, 0)
      expect(wx).toBe(0)
      expect(wy).toBe(10) // Top of world
    })

    it('converts canvas bottom-left correctly', () => {
      const [wx, wy] = transform.canvasToWorld(0, 600)
      expect(wx).toBe(0)
      expect(wy).toBe(0) // World origin
    })

    it('converts canvas bottom-right correctly', () => {
      const [wx, wy] = transform.canvasToWorld(600, 600)
      expect(wx).toBe(10)
      expect(wy).toBe(0)
    })

    it('converts canvas center correctly', () => {
      const [wx, wy] = transform.canvasToWorld(300, 300)
      expect(wx).toBe(5)
      expect(wy).toBe(5)
    })

    it('handles fractional pixel coordinates', () => {
      const [wx, wy] = transform.canvasToWorld(90, 450)
      expect(wx).toBe(1.5)
      expect(wy).toBe(2.5)
    })
  })

  describe('round-trip conversions', () => {
    const transform = new CoordinateTransform()

    it('world -> canvas -> world preserves coordinates', () => {
      const testPoints: Array<[number, number]> = [
        [0, 0],
        [5, 5],
        [10, 10],
        [2.5, 7.5],
        [0.1, 9.9],
      ]

      for (const [x, y] of testPoints) {
        const [cx, cy] = transform.worldToCanvas(x, y)
        const [wx, wy] = transform.canvasToWorld(cx, cy)
        expect(wx).toBeCloseTo(x, 10)
        expect(wy).toBeCloseTo(y, 10)
      }
    })

    it('canvas -> world -> canvas preserves coordinates', () => {
      const testPoints: Array<[number, number]> = [
        [0, 0],
        [300, 300],
        [600, 600],
        [150, 450],
        [599, 1],
      ]

      for (const [cx, cy] of testPoints) {
        const [wx, wy] = transform.canvasToWorld(cx, cy)
        const [ncx, ncy] = transform.worldToCanvas(wx, wy)
        expect(ncx).toBeCloseTo(cx, 10)
        expect(ncy).toBeCloseTo(cy, 10)
      }
    })
  })

  describe('worldThetaToCanvasRotation', () => {
    const transform = new CoordinateTransform()

    it('converts theta=0 (facing +X forward) to -90 degrees', () => {
      const rotation = transform.worldThetaToCanvasRotation(0)
      expect(rotation).toBe(-90)
    })

    it('converts theta=PI/2 (facing +Y left) to -180 degrees', () => {
      const rotation = transform.worldThetaToCanvasRotation(Math.PI / 2)
      expect(rotation).toBe(-180)
    })

    it('converts theta=PI (facing -X backward) to -270 degrees', () => {
      const rotation = transform.worldThetaToCanvasRotation(Math.PI)
      expect(rotation).toBe(-270)
    })

    it('converts theta=-PI/2 (facing -Y right) to 0 degrees', () => {
      const rotation = transform.worldThetaToCanvasRotation(-Math.PI / 2)
      expect(rotation).toBe(0)
    })

    it('handles arbitrary angles', () => {
      const rotation = transform.worldThetaToCanvasRotation(Math.PI / 4)
      expect(rotation).toBeCloseTo(-135, 10)
    })
  })

  describe('canvasRotationToWorldTheta', () => {
    const transform = new CoordinateTransform()

    it('converts -90 degrees to theta=0', () => {
      const theta = transform.canvasRotationToWorldTheta(-90)
      expect(theta).toBeCloseTo(0, 10)
    })

    it('converts -180 degrees to theta=PI/2', () => {
      const theta = transform.canvasRotationToWorldTheta(-180)
      expect(theta).toBeCloseTo(Math.PI / 2, 10)
    })

    it('converts -270 degrees to theta=PI', () => {
      const theta = transform.canvasRotationToWorldTheta(-270)
      expect(theta).toBeCloseTo(Math.PI, 10)
    })

    it('converts 0 degrees to theta=-PI/2', () => {
      const theta = transform.canvasRotationToWorldTheta(0)
      expect(theta).toBeCloseTo(-Math.PI / 2, 10)
    })
  })

  describe('theta round-trip conversions', () => {
    const transform = new CoordinateTransform()

    it('worldTheta -> canvasRotation -> worldTheta preserves angle', () => {
      const testAngles = [0, Math.PI / 4, Math.PI / 2, Math.PI, -Math.PI / 4, -Math.PI / 2]

      for (const theta of testAngles) {
        const rotation = transform.worldThetaToCanvasRotation(theta)
        const resultTheta = transform.canvasRotationToWorldTheta(rotation)
        expect(resultTheta).toBeCloseTo(theta, 10)
      }
    })
  })

  describe('transformPoint', () => {
    const transform = new CoordinateTransform()

    it('transforms with identity pose (no rotation, origin translation)', () => {
      const pose: Transform2D = { x: 0, y: 0, theta: 0 }
      const [tx, ty] = transform.transformPoint([1, 0], pose)
      expect(tx).toBeCloseTo(1, 10)
      expect(ty).toBeCloseTo(0, 10)
    })

    it('transforms with translation only', () => {
      const pose: Transform2D = { x: 5, y: 3, theta: 0 }
      const [tx, ty] = transform.transformPoint([1, 1], pose)
      expect(tx).toBeCloseTo(6, 10)
      expect(ty).toBeCloseTo(4, 10)
    })

    it('transforms with 90 degree rotation', () => {
      const pose: Transform2D = { x: 0, y: 0, theta: Math.PI / 2 }
      const [tx, ty] = transform.transformPoint([1, 0], pose)
      expect(tx).toBeCloseTo(0, 10)
      expect(ty).toBeCloseTo(1, 10)
    })

    it('transforms with 180 degree rotation', () => {
      const pose: Transform2D = { x: 0, y: 0, theta: Math.PI }
      const [tx, ty] = transform.transformPoint([1, 0], pose)
      expect(tx).toBeCloseTo(-1, 10)
      expect(ty).toBeCloseTo(0, 10)
    })

    it('transforms with -90 degree rotation', () => {
      const pose: Transform2D = { x: 0, y: 0, theta: -Math.PI / 2 }
      const [tx, ty] = transform.transformPoint([1, 0], pose)
      expect(tx).toBeCloseTo(0, 10)
      expect(ty).toBeCloseTo(-1, 10)
    })

    it('transforms with combined rotation and translation', () => {
      const pose: Transform2D = { x: 2, y: 3, theta: Math.PI / 2 }
      // Point (1, 0) rotated 90 degrees CCW becomes (0, 1), then translated by (2, 3)
      const [tx, ty] = transform.transformPoint([1, 0], pose)
      expect(tx).toBeCloseTo(2, 10)
      expect(ty).toBeCloseTo(4, 10)
    })

    it('transforms origin point', () => {
      const pose: Transform2D = { x: 5, y: 7, theta: Math.PI / 3 }
      const [tx, ty] = transform.transformPoint([0, 0], pose)
      expect(tx).toBeCloseTo(5, 10)
      expect(ty).toBeCloseTo(7, 10)
    })
  })

  describe('inverseTransformPoint', () => {
    const transform = new CoordinateTransform()

    it('inverse transforms with identity pose', () => {
      const pose: Transform2D = { x: 0, y: 0, theta: 0 }
      const [lx, ly] = transform.inverseTransformPoint([1, 0], pose)
      expect(lx).toBeCloseTo(1, 10)
      expect(ly).toBeCloseTo(0, 10)
    })

    it('inverse transforms with translation only', () => {
      const pose: Transform2D = { x: 5, y: 3, theta: 0 }
      const [lx, ly] = transform.inverseTransformPoint([6, 4], pose)
      expect(lx).toBeCloseTo(1, 10)
      expect(ly).toBeCloseTo(1, 10)
    })

    it('inverse transforms with 90 degree rotation', () => {
      const pose: Transform2D = { x: 0, y: 0, theta: Math.PI / 2 }
      const [lx, ly] = transform.inverseTransformPoint([0, 1], pose)
      expect(lx).toBeCloseTo(1, 10)
      expect(ly).toBeCloseTo(0, 10)
    })

    it('inverse transforms with combined rotation and translation', () => {
      const pose: Transform2D = { x: 2, y: 3, theta: Math.PI / 2 }
      // World point (2, 4) -> translate by (-2, -3) = (0, 1) -> rotate -90 = (1, 0)
      const [lx, ly] = transform.inverseTransformPoint([2, 4], pose)
      expect(lx).toBeCloseTo(1, 10)
      expect(ly).toBeCloseTo(0, 10)
    })
  })

  describe('transform/inverseTransform round-trip', () => {
    const transform = new CoordinateTransform()

    it('transform -> inverseTransform preserves point', () => {
      const poses: Transform2D[] = [
        { x: 0, y: 0, theta: 0 },
        { x: 5, y: 3, theta: Math.PI / 4 },
        { x: -2, y: 7, theta: -Math.PI / 3 },
        { x: 10, y: 10, theta: Math.PI },
      ]
      const points: Array<[number, number]> = [
        [0, 0],
        [1, 0],
        [0, 1],
        [1, 1],
        [-0.5, 2.5],
      ]

      for (const pose of poses) {
        for (const point of points) {
          const transformed = transform.transformPoint(point, pose)
          const recovered = transform.inverseTransformPoint(transformed, pose)
          expect(recovered[0]).toBeCloseTo(point[0], 10)
          expect(recovered[1]).toBeCloseTo(point[1], 10)
        }
      }
    })
  })

  describe('distance', () => {
    const transform = new CoordinateTransform()

    it('calculates zero distance for same point', () => {
      const d = transform.distance([3, 4], [3, 4])
      expect(d).toBe(0)
    })

    it('calculates horizontal distance', () => {
      const d = transform.distance([0, 0], [5, 0])
      expect(d).toBe(5)
    })

    it('calculates vertical distance', () => {
      const d = transform.distance([0, 0], [0, 5])
      expect(d).toBe(5)
    })

    it('calculates diagonal distance (3-4-5 triangle)', () => {
      const d = transform.distance([0, 0], [3, 4])
      expect(d).toBe(5)
    })

    it('calculates distance with negative coordinates', () => {
      const d = transform.distance([-3, -4], [0, 0])
      expect(d).toBe(5)
    })

    it('is symmetric', () => {
      const d1 = transform.distance([1, 2], [4, 6])
      const d2 = transform.distance([4, 6], [1, 2])
      expect(d1).toBe(d2)
    })
  })

  describe('normalizeAngle', () => {
    const transform = new CoordinateTransform()

    it('keeps angles within [-PI, PI] unchanged', () => {
      expect(transform.normalizeAngle(0)).toBe(0)
      expect(transform.normalizeAngle(Math.PI / 2)).toBeCloseTo(Math.PI / 2, 10)
      expect(transform.normalizeAngle(-Math.PI / 2)).toBeCloseTo(-Math.PI / 2, 10)
      expect(transform.normalizeAngle(Math.PI)).toBeCloseTo(Math.PI, 10)
      expect(transform.normalizeAngle(-Math.PI)).toBeCloseTo(-Math.PI, 10)
    })

    it('normalizes angles greater than PI', () => {
      expect(transform.normalizeAngle(3 * Math.PI / 2)).toBeCloseTo(-Math.PI / 2, 10)
      expect(transform.normalizeAngle(2 * Math.PI)).toBeCloseTo(0, 10)
      expect(transform.normalizeAngle(5 * Math.PI / 2)).toBeCloseTo(Math.PI / 2, 10)
    })

    it('normalizes angles less than -PI', () => {
      expect(transform.normalizeAngle(-3 * Math.PI / 2)).toBeCloseTo(Math.PI / 2, 10)
      expect(transform.normalizeAngle(-2 * Math.PI)).toBeCloseTo(0, 10)
      expect(transform.normalizeAngle(-5 * Math.PI / 2)).toBeCloseTo(-Math.PI / 2, 10)
    })

    it('handles large positive angles', () => {
      const normalized = transform.normalizeAngle(10 * Math.PI + Math.PI / 4)
      expect(normalized).toBeCloseTo(Math.PI / 4, 10)
    })

    it('handles large negative angles', () => {
      const normalized = transform.normalizeAngle(-10 * Math.PI - Math.PI / 4)
      expect(normalized).toBeCloseTo(-Math.PI / 4, 10)
    })
  })

  describe('defaultTransform singleton', () => {
    it('is a CoordinateTransform instance', () => {
      expect(defaultTransform).toBeInstanceOf(CoordinateTransform)
    })

    it('has default values', () => {
      expect(defaultTransform.worldSize).toBe(WORLD_SIZE)
      expect(defaultTransform.canvasSize).toBe(CANVAS_SIZE)
      expect(defaultTransform.scale).toBe(SCALE)
    })
  })

  describe('custom scale ratios', () => {
    it('handles non-square aspect ratios correctly', () => {
      const transform = new CoordinateTransform(20, 400) // 20 pixels per meter
      const [cx, cy] = transform.worldToCanvas(10, 10)
      expect(cx).toBe(200)
      expect(cy).toBe(200)

      const [wx, wy] = transform.canvasToWorld(200, 200)
      expect(wx).toBe(10)
      expect(wy).toBe(10)
    })

    it('handles very small scale', () => {
      const transform = new CoordinateTransform(100, 100) // 1 pixel per meter
      const [cx, cy] = transform.worldToCanvas(50, 50)
      expect(cx).toBe(50)
      expect(cy).toBe(50)
    })

    it('handles very large scale', () => {
      const transform = new CoordinateTransform(1, 1000) // 1000 pixels per meter
      const [cx, cy] = transform.worldToCanvas(0.5, 0.5)
      expect(cx).toBe(500)
      expect(cy).toBe(500)
    })
  })

  describe('edge cases', () => {
    const transform = new CoordinateTransform()

    it('handles very small numbers', () => {
      const [cx, cy] = transform.worldToCanvas(0.0001, 0.0001)
      expect(cx).toBeCloseTo(0.006, 5)
      expect(cy).toBeCloseTo(599.994, 2)
    })

    it('handles very large numbers', () => {
      const [cx, cy] = transform.worldToCanvas(1000, 1000)
      expect(cx).toBe(60000)
      expect(cy).toBe(-59400)
    })

    it('handles transformation at world boundaries', () => {
      // Test points exactly at world boundaries
      const corners: Array<[number, number]> = [
        [0, 0],
        [0, 10],
        [10, 0],
        [10, 10],
      ]

      const expectedCanvas: Array<[number, number]> = [
        [0, 600],
        [0, 0],
        [600, 600],
        [600, 0],
      ]

      for (let i = 0; i < corners.length; i++) {
        const [wx, wy] = corners[i]
        const [cx, cy] = transform.worldToCanvas(wx, wy)
        expect(cx).toBe(expectedCanvas[i][0])
        expect(cy).toBe(expectedCanvas[i][1])
      }
    })
  })
})
