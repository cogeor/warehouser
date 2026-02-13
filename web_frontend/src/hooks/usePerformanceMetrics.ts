import { useEffect, useRef, useState } from 'react';

/**
 * Performance metrics tracked by the hook.
 */
export interface PerformanceMetrics {
  /** Frames per second */
  fps: number;
  /** Average milliseconds per frame */
  frameTime: number;
  /** Memory usage in MB (Chrome only, undefined in other browsers) */
  memoryUsage?: number;
  /** Number of times the hook has updated metrics */
  renderCount: number;
}

/**
 * Extended Performance interface with Chrome-specific memory info.
 * This is only available in Chrome/Chromium browsers.
 */
interface PerformanceWithMemory extends Performance {
  memory?: {
    usedJSHeapSize: number;
    totalJSHeapSize: number;
    jsHeapSizeLimit: number;
  };
}

const DEFAULT_UPDATE_INTERVAL = 1000;

/**
 * Hook for tracking performance metrics including FPS, frame time, and memory usage.
 *
 * Uses requestAnimationFrame to accurately measure frame timing.
 * Memory usage is only available in Chrome/Chromium browsers.
 *
 * @param updateInterval - How often to update the returned metrics in milliseconds (default: 1000ms)
 * @returns Current performance metrics
 *
 * @example
 * ```tsx
 * function PerformanceOverlay() {
 *   const metrics = usePerformanceMetrics(500);
 *
 *   return (
 *     <div className="performance-overlay">
 *       <div>FPS: {metrics.fps.toFixed(1)}</div>
 *       <div>Frame Time: {metrics.frameTime.toFixed(2)}ms</div>
 *       {metrics.memoryUsage !== undefined && (
 *         <div>Memory: {metrics.memoryUsage.toFixed(1)}MB</div>
 *       )}
 *     </div>
 *   );
 * }
 * ```
 */
export function usePerformanceMetrics(
  updateInterval: number = DEFAULT_UPDATE_INTERVAL
): PerformanceMetrics {
  const [metrics, setMetrics] = useState<PerformanceMetrics>({
    fps: 0,
    frameTime: 0,
    memoryUsage: undefined,
    renderCount: 0,
  });

  // Mutable refs for tracking frame data between animation frames
  const frameCountRef = useRef(0);
  const lastTimeRef = useRef(performance.now());
  const frameTimesRef = useRef<number[]>([]);
  const animationFrameIdRef = useRef<number | null>(null);
  const lastUpdateTimeRef = useRef(performance.now());
  const renderCountRef = useRef(0);

  useEffect(() => {
    let previousFrameTime = performance.now();

    const measureFrame = (currentTime: number): void => {
      // Calculate time since last frame
      const frameDelta = currentTime - previousFrameTime;
      previousFrameTime = currentTime;

      // Track frame times for averaging
      frameTimesRef.current.push(frameDelta);
      frameCountRef.current++;

      // Check if it's time to update metrics
      const timeSinceLastUpdate = currentTime - lastUpdateTimeRef.current;

      if (timeSinceLastUpdate >= updateInterval) {
        const elapsedTime = currentTime - lastTimeRef.current;
        const frameCount = frameCountRef.current;
        const frameTimes = frameTimesRef.current;

        // Calculate FPS
        const fps = frameCount > 0 ? (frameCount / elapsedTime) * 1000 : 0;

        // Calculate average frame time
        const averageFrameTime =
          frameTimes.length > 0
            ? frameTimes.reduce((sum, time) => sum + time, 0) / frameTimes.length
            : 0;

        // Get memory usage if available (Chrome only)
        const performanceWithMemory = performance as PerformanceWithMemory;
        const memoryUsage = performanceWithMemory.memory
          ? performanceWithMemory.memory.usedJSHeapSize / (1024 * 1024)
          : undefined;

        // Increment render count
        renderCountRef.current++;

        // Update state
        setMetrics({
          fps,
          frameTime: averageFrameTime,
          memoryUsage,
          renderCount: renderCountRef.current,
        });

        // Reset tracking for next interval
        frameCountRef.current = 0;
        frameTimesRef.current = [];
        lastTimeRef.current = currentTime;
        lastUpdateTimeRef.current = currentTime;
      }

      // Schedule next frame
      animationFrameIdRef.current = requestAnimationFrame(measureFrame);
    };

    // Start the animation frame loop
    animationFrameIdRef.current = requestAnimationFrame(measureFrame);

    // Cleanup on unmount
    return () => {
      if (animationFrameIdRef.current !== null) {
        cancelAnimationFrame(animationFrameIdRef.current);
        animationFrameIdRef.current = null;
      }
    };
  }, [updateInterval]);

  return metrics;
}
