/**
 * useRosTopic - React hooks for subscribing to ROS topics
 *
 * Provides hooks for subscribing to ROS topics with automatic lifecycle
 * management (subscribe on connect, unsubscribe on disconnect/unmount).
 */

import { useState, useEffect, useRef, useCallback } from 'react';
import { useRosConnection } from './useRosConnection';
import { createTypedSubscription } from '../ros/subscriptions';
import type { WorldState, LidarDebug, TaskStatus } from '../types/warehouser_msgs';

// =============================================================================
// Types
// =============================================================================

/** Return type for useRosTopic hook */
interface UseRosTopicResult<T> {
  /** Latest received message data, or null if no message received yet */
  data: T | null;
  /** Whether currently subscribed to the topic */
  isSubscribed: boolean;
}

// =============================================================================
// Generic Hooks
// =============================================================================

/**
 * Hook to subscribe to a ROS topic with automatic lifecycle management.
 *
 * Subscribes when the ROS connection is established and unsubscribes
 * on unmount or when the connection is lost.
 *
 * @typeParam T - The message type for this topic
 * @param topicName - The name of the ROS topic to subscribe to
 * @param messageType - The ROS message type string (e.g., 'warehouser_msgs/msg/WorldState')
 * @returns Object containing the latest data and subscription status
 *
 * @example
 * ```tsx
 * function WorldDisplay() {
 *   const { data, isSubscribed } = useRosTopic<WorldState>(
 *     '/world/state',
 *     'warehouser_msgs/msg/WorldState'
 *   );
 *
 *   if (!isSubscribed) return <span>Waiting for connection...</span>;
 *   if (!data) return <span>Waiting for data...</span>;
 *
 *   return <span>Entities: {data.entities.length}</span>;
 * }
 * ```
 */
export function useRosTopic<T>(
  topicName: string,
  messageType: string
): UseRosTopicResult<T> {
  const { ros, isConnected } = useRosConnection();
  const [data, setData] = useState<T | null>(null);
  const [isSubscribed, setIsSubscribed] = useState<boolean>(false);

  useEffect(() => {
    // Only subscribe when connected and ros instance is available
    if (!isConnected || ros === null) {
      setIsSubscribed(false);
      return;
    }

    // Create subscription
    const unsubscribe = createTypedSubscription<T>(
      ros,
      topicName,
      messageType,
      (message: T) => {
        setData(message);
      }
    );

    setIsSubscribed(true);

    // Cleanup: unsubscribe when unmount or connection lost
    return () => {
      unsubscribe();
      setIsSubscribed(false);
    };
  }, [ros, isConnected, topicName, messageType]);

  return { data, isSubscribed };
}

/**
 * Hook to subscribe to a ROS topic with throttled updates.
 *
 * Same as useRosTopic but limits how often the state is updated.
 * Useful for high-frequency topics like lidar that would otherwise
 * cause excessive re-renders.
 *
 * @typeParam T - The message type for this topic
 * @param topicName - The name of the ROS topic to subscribe to
 * @param messageType - The ROS message type string
 * @param throttleMs - Minimum time between state updates in milliseconds
 * @returns Object containing the latest data and subscription status
 *
 * @example
 * ```tsx
 * function LidarDisplay() {
 *   // Update at most every 100ms (10 Hz)
 *   const { data, isSubscribed } = useThrottledTopic<LidarDebug>(
 *     '/observations/lidar_debug',
 *     'warehouser_msgs/msg/LidarDebug',
 *     100
 *   );
 *
 *   if (!data) return null;
 *   return <LidarVisualization ranges={data.ranges} />;
 * }
 * ```
 */
export function useThrottledTopic<T>(
  topicName: string,
  messageType: string,
  throttleMs: number
): UseRosTopicResult<T> {
  const { ros, isConnected } = useRosConnection();
  const [data, setData] = useState<T | null>(null);
  const [isSubscribed, setIsSubscribed] = useState<boolean>(false);

  // Track last update time using a ref (doesn't trigger re-renders)
  const lastUpdateRef = useRef<number>(0);

  // Throttled callback that only updates state if enough time has passed
  const handleMessage = useCallback(
    (message: T) => {
      const now = Date.now();
      if (now - lastUpdateRef.current >= throttleMs) {
        lastUpdateRef.current = now;
        setData(message);
      }
    },
    [throttleMs]
  );

  useEffect(() => {
    // Only subscribe when connected and ros instance is available
    if (!isConnected || ros === null) {
      setIsSubscribed(false);
      return;
    }

    // Create subscription with throttled callback
    const unsubscribe = createTypedSubscription<T>(
      ros,
      topicName,
      messageType,
      handleMessage
    );

    setIsSubscribed(true);

    // Cleanup: unsubscribe when unmount or connection lost
    return () => {
      unsubscribe();
      setIsSubscribed(false);
    };
  }, [ros, isConnected, topicName, messageType, handleMessage]);

  return { data, isSubscribed };
}

// =============================================================================
// Convenience Hooks
// =============================================================================

/**
 * Hook to subscribe to the /world/state topic.
 *
 * @returns WorldState data and subscription status
 *
 * @example
 * ```tsx
 * function WorldView() {
 *   const { data: worldState, isSubscribed } = useWorldState();
 *
 *   if (!worldState) return null;
 *
 *   return (
 *     <div>
 *       <p>Simulation time: {worldState.sim_time.toFixed(2)}s</p>
 *       <p>Running: {worldState.running ? 'Yes' : 'No'}</p>
 *       <p>Entity count: {worldState.entities.length}</p>
 *     </div>
 *   );
 * }
 * ```
 */
export function useWorldState(): UseRosTopicResult<WorldState> {
  return useRosTopic<WorldState>('/world/state', 'warehouser_msgs/msg/WorldState');
}

/**
 * Hook to subscribe to the /observations/lidar_debug topic.
 *
 * Lidar data can be published at high frequency, so this hook supports
 * optional throttling to prevent excessive re-renders.
 *
 * @param throttleMs - Optional minimum time between updates in milliseconds.
 *                     If not provided, every message triggers an update.
 * @returns LidarDebug data and subscription status
 *
 * @example
 * ```tsx
 * function LidarOverlay() {
 *   // Throttle to 20 Hz to reduce render load
 *   const { data: lidar } = useLidarDebug(50);
 *
 *   if (!lidar) return null;
 *
 *   return <LidarCanvas ranges={lidar.ranges} />;
 * }
 * ```
 */
export function useLidarDebug(throttleMs?: number): UseRosTopicResult<LidarDebug> {
  // Use throttled version if throttleMs is specified, otherwise use regular
  const throttledResult = useThrottledTopic<LidarDebug>(
    '/observations/lidar_debug',
    'warehouser_msgs/msg/LidarDebug',
    throttleMs ?? 0
  );

  const regularResult = useRosTopic<LidarDebug>(
    '/observations/lidar_debug',
    'warehouser_msgs/msg/LidarDebug'
  );

  // Return throttled result if throttleMs is specified (including 0)
  // Return regular result only if throttleMs is undefined
  return throttleMs !== undefined ? throttledResult : regularResult;
}

/**
 * Hook to subscribe to the /task/status topic.
 *
 * @returns TaskStatus data and subscription status
 *
 * @example
 * ```tsx
 * function TaskDisplay() {
 *   const { data: task, isSubscribed } = useTaskStatus();
 *
 *   if (!task) return <span>No active task</span>;
 *
 *   return (
 *     <div>
 *       <p>Task: {task.task_id}</p>
 *       <p>State: {task.state}</p>
 *       <p>Target: {task.target_color}</p>
 *       <p>Distance: {task.distance_to_goal.toFixed(2)}m</p>
 *     </div>
 *   );
 * }
 * ```
 */
export function useTaskStatus(): UseRosTopicResult<TaskStatus> {
  return useRosTopic<TaskStatus>('/task/status', 'warehouser_msgs/msg/TaskStatus');
}

// =============================================================================
// Exports
// =============================================================================

export type { UseRosTopicResult };
