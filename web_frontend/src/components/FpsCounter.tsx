/**
 * FpsCounter - Displays current frames per second for performance monitoring
 *
 * Uses requestAnimationFrame to accurately track frame rate with color-coded
 * display based on performance thresholds.
 */

import { useEffect, useRef, useState } from 'react';

// =============================================================================
// Types
// =============================================================================

/** Props for FpsCounter component */
interface FpsCounterProps {
  /** Additional CSS classes to apply to the container */
  className?: string;
  /** How often to update the FPS display in milliseconds (default: 500ms) */
  updateInterval?: number;
}

// =============================================================================
// Constants
// =============================================================================

/** Default update interval in milliseconds */
const DEFAULT_UPDATE_INTERVAL = 500;

/** FPS threshold for green color (good performance) */
const FPS_THRESHOLD_GREEN = 50;

/** FPS threshold for yellow color (acceptable performance) */
const FPS_THRESHOLD_YELLOW = 30;

// =============================================================================
// Component
// =============================================================================

/**
 * FPS counter component for debugging and performance monitoring.
 *
 * Tracks frame rate using requestAnimationFrame and displays the current FPS
 * with color coding based on performance:
 * - Green (>= 50 FPS): Good performance
 * - Yellow (>= 30 FPS): Acceptable performance
 * - Red (< 30 FPS): Poor performance
 *
 * @example
 * ```tsx
 * <FpsCounter />
 * <FpsCounter updateInterval={1000} />
 * <FpsCounter className="absolute top-2 right-2" />
 * ```
 */
export function FpsCounter({
  className = '',
  updateInterval = DEFAULT_UPDATE_INTERVAL,
}: FpsCounterProps): JSX.Element {
  const [fps, setFps] = useState<number>(0);

  // Refs for tracking frame count and animation frame handle
  const frameCountRef = useRef<number>(0);
  const animationFrameRef = useRef<number>(0);

  useEffect(() => {
    let lastUpdateTime = performance.now();

    const countFrame = (currentTime: number): void => {
      frameCountRef.current += 1;

      // Check if it's time to update the display
      const elapsed = currentTime - lastUpdateTime;
      if (elapsed >= updateInterval) {
        // Calculate FPS: frames / (elapsed time in seconds)
        const calculatedFps = Math.round(
          (frameCountRef.current * 1000) / elapsed
        );
        setFps(calculatedFps);

        // Reset counters
        frameCountRef.current = 0;
        lastUpdateTime = currentTime;
      }

      // Continue the animation loop
      animationFrameRef.current = requestAnimationFrame(countFrame);
    };

    // Start the animation loop
    animationFrameRef.current = requestAnimationFrame(countFrame);

    // Cleanup on unmount
    return () => {
      if (animationFrameRef.current) {
        cancelAnimationFrame(animationFrameRef.current);
      }
    };
  }, [updateInterval]);

  // Determine color class based on FPS
  const getColorClass = (): string => {
    if (fps >= FPS_THRESHOLD_GREEN) {
      return 'text-green-400';
    }
    if (fps >= FPS_THRESHOLD_YELLOW) {
      return 'text-yellow-400';
    }
    return 'text-red-400';
  };

  return (
    <span className={`text-sm font-mono ${getColorClass()} ${className}`}>
      {fps} FPS
    </span>
  );
}

// =============================================================================
// Exports
// =============================================================================

export type { FpsCounterProps };
