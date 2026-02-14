/**
 * useEntityAnimation - React hooks for animating Konva elements
 *
 * Provides hooks for smoothly animating Konva nodes to target positions
 * using Konva's built-in tweening capabilities.
 */

import { useRef, useEffect, useCallback } from 'react';
import Konva from 'konva';
import { CANVAS_CONFIG } from '../config';

// =============================================================================
// Types
// =============================================================================

/** Target position and rotation for animation */
export interface AnimationTarget {
  /** Target X position in canvas coordinates */
  x: number;
  /** Target Y position in canvas coordinates */
  y: number;
  /** Target rotation in degrees (optional) */
  rotation?: number;
}

/** Options for animation behavior */
export interface AnimationOptions {
  /** Animation duration in milliseconds */
  duration?: number;
  /** Easing function from Konva.Easings */
  easing?: (t: number, b: number, c: number, d: number) => number;
}

// =============================================================================
// useEntityAnimation
// =============================================================================

/**
 * Hook that animates a Konva node to a target position.
 *
 * Positions the node immediately on first render, then animates
 * subsequent position changes using Konva.to().
 *
 * @typeParam T - The Konva node type (e.g., Konva.Circle, Konva.Rect)
 * @param target - Target position and optional rotation
 * @param options - Animation options (duration, easing)
 * @returns Ref to attach to the Konva node
 *
 * @example
 * ```tsx
 * import { Circle } from 'react-konva';
 * import { useEntityAnimation } from '../hooks/useEntityAnimation';
 *
 * function AnimatedRobot({ x, y, rotation }: { x: number; y: number; rotation: number }) {
 *   const ref = useEntityAnimation<Konva.Circle>({ x, y, rotation });
 *
 *   return (
 *     <Circle
 *       ref={ref}
 *       radius={30}
 *       fill="blue"
 *     />
 *   );
 * }
 * ```
 */
export function useEntityAnimation<T extends Konva.Node>(
  target: AnimationTarget,
  options?: AnimationOptions
): React.RefObject<T | null> {
  const nodeRef = useRef<T | null>(null);
  const isFirstRender = useRef<boolean>(true);

  const duration = options?.duration ?? CANVAS_CONFIG.ANIMATION_DURATION;
  const easing = options?.easing ?? Konva.Easings.EaseOut;

  useEffect(() => {
    const node = nodeRef.current;
    if (!node) return;

    if (isFirstRender.current) {
      // First render: position immediately without animation
      node.x(target.x);
      node.y(target.y);
      if (target.rotation !== undefined) {
        node.rotation(target.rotation);
      }
      isFirstRender.current = false;
    } else {
      // Subsequent renders: animate to target position
      const tweenConfig: Konva.TweenConfig = {
        node,
        x: target.x,
        y: target.y,
        duration: duration / 1000, // Konva uses seconds
        easing,
      };

      if (target.rotation !== undefined) {
        tweenConfig.rotation = target.rotation;
      }

      const tween = new Konva.Tween(tweenConfig);
      tween.play();

      // Cleanup: stop animation if component updates before animation completes
      return () => {
        tween.destroy();
      };
    }
  }, [target.x, target.y, target.rotation, duration, easing]);

  return nodeRef;
}

// =============================================================================
// useMultipleEntityAnimations
// =============================================================================

/**
 * Hook that manages animations for multiple Konva nodes by ID.
 *
 * Useful when rendering a dynamic list of entities that need independent
 * animation tracking.
 *
 * @typeParam T - The Konva node type (e.g., Konva.Circle, Konva.Rect)
 * @param targets - Map of entity IDs to their target positions
 * @returns Object with getRef callback and refs map
 *
 * @example
 * ```tsx
 * import { Circle } from 'react-konva';
 * import { useMultipleEntityAnimations } from '../hooks/useEntityAnimation';
 *
 * function AnimatedEntities({ entities }: { entities: Entity[] }) {
 *   const targets = new Map(
 *     entities.map(e => [e.id, { x: e.x, y: e.y }])
 *   );
 *   const { getRef } = useMultipleEntityAnimations<Konva.Circle>(targets);
 *
 *   return (
 *     <>
 *       {entities.map(entity => (
 *         <Circle
 *           key={entity.id}
 *           ref={getRef(entity.id)}
 *           radius={20}
 *           fill="green"
 *         />
 *       ))}
 *     </>
 *   );
 * }
 * ```
 */
export function useMultipleEntityAnimations<T extends Konva.Node>(
  targets: Map<string, AnimationTarget>,
  options?: AnimationOptions
): {
  getRef: (id: string) => (node: T | null) => void;
  refs: Map<string, T>;
} {
  const refsMap = useRef<Map<string, T>>(new Map());
  const firstRenderMap = useRef<Map<string, boolean>>(new Map());
  const tweensMap = useRef<Map<string, Konva.Tween>>(new Map());

  const duration = options?.duration ?? CANVAS_CONFIG.ANIMATION_DURATION;
  const easing = options?.easing ?? Konva.Easings.EaseOut;

  // Update animations when targets change
  useEffect(() => {
    // Capture current ref values for cleanup
    const currentTweensMap = tweensMap.current;

    targets.forEach((target, id) => {
      const node = refsMap.current.get(id);
      if (!node) return;

      const isFirst = !firstRenderMap.current.has(id);

      if (isFirst) {
        // First time seeing this entity: position immediately
        node.x(target.x);
        node.y(target.y);
        if (target.rotation !== undefined) {
          node.rotation(target.rotation);
        }
        firstRenderMap.current.set(id, true);
      } else {
        // Subsequent updates: animate to target
        // Stop any existing animation for this entity
        const existingTween = currentTweensMap.get(id);
        if (existingTween) {
          existingTween.destroy();
        }

        const tweenConfig: Konva.TweenConfig = {
          node,
          x: target.x,
          y: target.y,
          duration: duration / 1000,
          easing,
        };

        if (target.rotation !== undefined) {
          tweenConfig.rotation = target.rotation;
        }

        const tween = new Konva.Tween(tweenConfig);
        currentTweensMap.set(id, tween);
        tween.play();
      }
    });

    // Cleanup entities that are no longer in targets
    const targetIds = new Set(targets.keys());
    for (const id of firstRenderMap.current.keys()) {
      if (!targetIds.has(id)) {
        firstRenderMap.current.delete(id);
        const tween = currentTweensMap.get(id);
        if (tween) {
          tween.destroy();
          currentTweensMap.delete(id);
        }
        refsMap.current.delete(id);
      }
    }

    // Cleanup all tweens on unmount
    return () => {
      currentTweensMap.forEach((tween) => tween.destroy());
    };
  }, [targets, duration, easing]);

  // Callback to get a ref setter for a specific entity ID
  const getRef = useCallback((id: string) => {
    return (node: T | null) => {
      if (node) {
        refsMap.current.set(id, node);
      } else {
        refsMap.current.delete(id);
      }
    };
  }, []);

  return {
    getRef,
    refs: refsMap.current,
  };
}
