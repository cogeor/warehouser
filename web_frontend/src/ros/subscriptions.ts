import ROSLIB from 'roslib';
import { WorldState, LidarDebug, TaskStatus } from '../types/warehouser_msgs';

/**
 * Function type for unsubscribing from a topic
 */
export type Unsubscribe = () => void;

/**
 * Callback type for topic message handlers
 */
export type MessageCallback<T> = (message: T) => void;

/**
 * Creates a typed subscription to a ROS topic
 *
 * @param ros - The ROSLIB.Ros connection instance
 * @param topicName - The name of the topic to subscribe to
 * @param messageType - The ROS message type string
 * @param callback - Callback function to handle incoming messages
 * @returns An unsubscribe function to stop receiving messages
 */
export function createTypedSubscription<T>(
  ros: ROSLIB.Ros,
  topicName: string,
  messageType: string,
  callback: MessageCallback<T>
): Unsubscribe {
  const topic = new ROSLIB.Topic<T>({
    ros,
    name: topicName,
    messageType,
  });

  topic.subscribe(callback);

  return () => {
    topic.unsubscribe();
  };
}

/**
 * Creates a subscription to the /world/state topic
 *
 * @param ros - The ROSLIB.Ros connection instance
 * @param callback - Callback function to handle WorldState messages
 * @returns An unsubscribe function to stop receiving messages
 */
export function createWorldStateSubscription(
  ros: ROSLIB.Ros,
  callback: MessageCallback<WorldState>
): Unsubscribe {
  return createTypedSubscription<WorldState>(
    ros,
    '/world/state',
    'warehouser_msgs/WorldState',
    callback
  );
}

/**
 * Creates a subscription to the /observations/lidar_debug topic
 *
 * @param ros - The ROSLIB.Ros connection instance
 * @param callback - Callback function to handle LidarDebug messages
 * @returns An unsubscribe function to stop receiving messages
 */
export function createLidarDebugSubscription(
  ros: ROSLIB.Ros,
  callback: MessageCallback<LidarDebug>
): Unsubscribe {
  return createTypedSubscription<LidarDebug>(
    ros,
    '/observations/lidar_debug',
    'warehouser_msgs/LidarDebug',
    callback
  );
}

/**
 * Creates a subscription to the /task/status topic
 *
 * @param ros - The ROSLIB.Ros connection instance
 * @param callback - Callback function to handle TaskStatus messages
 * @returns An unsubscribe function to stop receiving messages
 */
export function createTaskStatusSubscription(
  ros: ROSLIB.Ros,
  callback: MessageCallback<TaskStatus>
): Unsubscribe {
  return createTypedSubscription<TaskStatus>(
    ros,
    '/task/status',
    'warehouser_msgs/TaskStatus',
    callback
  );
}
