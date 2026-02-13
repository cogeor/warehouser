/**
 * RosDataBridge - Bridges new ROS hooks to Zustand store
 *
 * This component subscribes to ROS topics using the new hook system
 * and updates the legacy Zustand appStore so existing components
 * (Canvas, panels) receive data without modification.
 *
 * Uses shallow comparison to avoid unnecessary store updates and re-renders.
 */

import { useEffect, useRef } from 'react';
import { useThrottledTopic, useTaskStatus } from '../hooks/useRosTopic';
import { useRosConnection } from '../hooks/useRosConnection';
import { useAppStore, Entity } from '../store/appStore';
import type { WorldState, LidarDebug } from '../types/warehouser_msgs';

/**
 * Maps ROS entity type enum to string type
 */
const ENTITY_TYPE_MAP: Record<number, Entity['type']> = {
  0: 'robot',
  1: 'object',
  2: 'wall',
  3: 'zone',
};

/**
 * Shallow compare two entity arrays - returns true if they have same content
 */
function entitiesEqual(a: Entity[], b: Entity[]): boolean {
  if (a.length !== b.length) return false;
  for (let i = 0; i < a.length; i++) {
    const ea = a[i];
    const eb = b[i];
    if (
      ea.id !== eb.id ||
      ea.type !== eb.type ||
      ea.x !== eb.x ||
      ea.y !== eb.y ||
      ea.theta !== eb.theta ||
      ea.color !== eb.color ||
      ea.width !== eb.width ||
      ea.height !== eb.height ||
      ea.isCarrying !== eb.isCarrying ||
      ea.carriedId !== eb.carriedId
    ) {
      return false;
    }
  }
  return true;
}

/**
 * Shallow compare two number arrays
 */
function arraysEqual(a: number[], b: number[]): boolean {
  if (a.length !== b.length) return false;
  for (let i = 0; i < a.length; i++) {
    if (a[i] !== b[i]) return false;
  }
  return true;
}

/**
 * Bridge component that syncs ROS topic data to Zustand store.
 * Renders nothing - purely for side effects.
 */
export function RosDataBridge(): null {
  const { isConnected } = useRosConnection();

  // Throttle world state to 10Hz (100ms) - sufficient for smooth visualization
  const { data: worldState } = useThrottledTopic<WorldState>(
    '/world/state',
    'warehouser_msgs/msg/WorldState',
    100
  );

  // Throttle lidar to 10Hz (100ms)
  const { data: lidarDebug } = useThrottledTopic<LidarDebug>(
    '/observations/lidar_debug',
    'warehouser_msgs/msg/LidarDebug',
    100
  );

  const { data: taskStatus } = useTaskStatus();

  // Track previous values to avoid redundant updates
  const prevEntitiesRef = useRef<Entity[]>([]);
  const prevLidarRef = useRef<{ ranges: number[]; angleMin: number; angleMax: number }>({
    ranges: [],
    angleMin: 0,
    angleMax: 0,
  });
  const prevTaskRef = useRef<{ state: string; intent: string }>({ state: '', intent: '' });

  // Sync connection status
  useEffect(() => {
    useAppStore.getState().setConnected(isConnected);
  }, [isConnected]);

  // Sync world state to store (only when entities actually change)
  useEffect(() => {
    if (!worldState) return;

    const entities: Entity[] = worldState.entities.map((e) => ({
      id: e.id,
      type: ENTITY_TYPE_MAP[e.type] ?? 'object',
      x: e.x,
      y: e.y,
      theta: e.theta,
      color: e.color,
      width: e.width,
      height: e.height,
      isCarrying: e.is_carrying,
      carriedId: e.carried_object_id,
    }));

    // Only update if entities actually changed
    if (!entitiesEqual(entities, prevEntitiesRef.current)) {
      prevEntitiesRef.current = entities;
      useAppStore.getState().setEntities(entities);
    }

    useAppStore.getState().setSimTime(worldState.sim_time);
  }, [worldState]);

  // Sync lidar data to store (only when data changes)
  useEffect(() => {
    if (!lidarDebug) return;

    const prev = prevLidarRef.current;
    const rangesChanged = !arraysEqual(lidarDebug.ranges, prev.ranges);
    const anglesChanged =
      lidarDebug.angle_min !== prev.angleMin || lidarDebug.angle_max !== prev.angleMax;

    if (rangesChanged || anglesChanged) {
      prevLidarRef.current = {
        ranges: lidarDebug.ranges,
        angleMin: lidarDebug.angle_min,
        angleMax: lidarDebug.angle_max,
      };
      useAppStore.getState().setLidar(lidarDebug.ranges, lidarDebug.angle_min, lidarDebug.angle_max);
    }
  }, [lidarDebug]);

  // Sync task status to store (only when changed)
  useEffect(() => {
    if (!taskStatus) return;

    const prev = prevTaskRef.current;
    const intent = taskStatus.intent ?? '';
    if (taskStatus.state !== prev.state || intent !== prev.intent) {
      prevTaskRef.current = { state: taskStatus.state, intent };
      useAppStore.getState().setTaskStatus(taskStatus.state, intent);
    }
  }, [taskStatus]);

  // This component renders nothing
  return null;
}
