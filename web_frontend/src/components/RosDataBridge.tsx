/**
 * RosDataBridge - Bridges new ROS hooks to Zustand store
 *
 * This component subscribes to ROS topics using the new hook system
 * and updates the legacy Zustand appStore so existing components
 * (Canvas, panels) receive data without modification.
 */

import { useEffect } from 'react';
import { useWorldState, useLidarDebug, useTaskStatus } from '../hooks/useRosTopic';
import { useRosConnection } from '../hooks/useRosConnection';
import { useAppStore, Entity } from '../store/appStore';

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
 * Bridge component that syncs ROS topic data to Zustand store.
 * Renders nothing - purely for side effects.
 */
export function RosDataBridge(): null {
  const { isConnected } = useRosConnection();
  const { data: worldState } = useWorldState();
  const { data: lidarDebug } = useLidarDebug(50); // Throttle to 20Hz
  const { data: taskStatus } = useTaskStatus();

  // Store setters
  const setConnected = useAppStore((s) => s.setConnected);
  const setEntities = useAppStore((s) => s.setEntities);
  const setSimTime = useAppStore((s) => s.setSimTime);
  const setLidar = useAppStore((s) => s.setLidar);
  const setTaskStatus = useAppStore((s) => s.setTaskStatus);

  // Sync connection status
  useEffect(() => {
    setConnected(isConnected);
  }, [isConnected, setConnected]);

  // Sync world state to store
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

    setEntities(entities);
    setSimTime(worldState.sim_time);
  }, [worldState, setEntities, setSimTime]);

  // Sync lidar data to store
  useEffect(() => {
    if (!lidarDebug) return;
    setLidar(lidarDebug.ranges, lidarDebug.angle_min, lidarDebug.angle_max);
  }, [lidarDebug, setLidar]);

  // Sync task status to store
  useEffect(() => {
    if (!taskStatus) return;
    setTaskStatus(taskStatus.state, taskStatus.intent ?? '');
  }, [taskStatus, setTaskStatus]);

  // This component renders nothing
  return null;
}
