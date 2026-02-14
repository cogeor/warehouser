/**
 * @deprecated This module is deprecated and will be removed in a future version.
 *
 * Please migrate to the new modular ROS connection system:
 * - RosConnection class: import from './RosConnection'
 * - Topic subscriptions: import from './subscriptions'
 * - React hooks: import from '../hooks/useRosConnection'
 *
 * This module is kept for backwards compatibility during migration.
 */

import ROSLIB from 'roslib'
import { useAppStore, Entity } from '../store/appStore'

let ros: ROSLIB.Ros | null = null
let reconnectTimeoutId: ReturnType<typeof setTimeout> | null = null

// Reconnection configuration
const RECONNECT_CONFIG = {
  baseDelay: 1000,      // 1 second
  maxDelay: 30000,      // 30 seconds
  factor: 2,
  jitter: 0.1,          // +/- 10%
  maxAttempts: 10,
}

/**
 * Calculate exponential backoff delay with jitter
 *
 * @deprecated Use RosConnection class from './RosConnection' instead.
 * The RosConnection class handles backoff internally.
 * This function will be removed in a future version.
 */
export function calculateBackoffDelay(attempt: number): number {
  // Exponential backoff: baseDelay * factor^attempt
  const exponentialDelay = RECONNECT_CONFIG.baseDelay * Math.pow(RECONNECT_CONFIG.factor, attempt)

  // Cap at max delay
  const cappedDelay = Math.min(exponentialDelay, RECONNECT_CONFIG.maxDelay)

  // Add jitter: +/- 10%
  const jitterRange = cappedDelay * RECONNECT_CONFIG.jitter
  const jitter = (Math.random() * 2 - 1) * jitterRange

  return Math.round(cappedDelay + jitter)
}

/**
 * Attempt to reconnect with exponential backoff
 */
function scheduleReconnect() {
  const store = useAppStore.getState()
  const currentAttempt = store.reconnectAttempt

  // Check if max attempts reached
  if (currentAttempt >= RECONNECT_CONFIG.maxAttempts) {
    console.error('Max reconnection attempts reached')
    store.setConnectionError('Connection failed after maximum retry attempts. Click to retry.')
    return
  }

  const delay = calculateBackoffDelay(currentAttempt)
  console.log(`Reconnection attempt ${currentAttempt + 1}/${RECONNECT_CONFIG.maxAttempts} in ${delay}ms`)

  // Clear any existing timeout
  if (reconnectTimeoutId) {
    clearTimeout(reconnectTimeoutId)
  }

  reconnectTimeoutId = setTimeout(() => {
    if (ros) {
      store.setReconnectAttempt(currentAttempt + 1)
      ros.connect('ws://localhost:9090')
    }
  }, delay)
}

/**
 * Reset reconnection state (called on successful connection)
 */
function resetReconnectionState() {
  const store = useAppStore.getState()
  store.setReconnectAttempt(0)
  store.setConnectionError(null)

  if (reconnectTimeoutId) {
    clearTimeout(reconnectTimeoutId)
    reconnectTimeoutId = null
  }
}

/**
 * Manual retry connection (for user-initiated retry)
 *
 * @deprecated Use RosConnection class from './RosConnection' instead.
 * Call rosConnection.connect() to initiate or retry connection.
 * This function will be removed in a future version.
 */
export function retryConnection() {
  const store = useAppStore.getState()
  store.setReconnectAttempt(0)
  store.setConnectionError(null)

  if (reconnectTimeoutId) {
    clearTimeout(reconnectTimeoutId)
    reconnectTimeoutId = null
  }

  if (ros) {
    ros.connect('ws://localhost:9090')
  } else {
    initRosConnection()
  }
}

/**
 * Initialize the ROS connection
 *
 * @deprecated Use RosConnection class from './RosConnection' instead.
 * Create a new RosConnection instance and call connect().
 * For React components, use the useRosConnection hook from '../hooks/useRosConnection'.
 * This function will be removed in a future version.
 */
export function initRosConnection() {
  const store = useAppStore.getState()

  ros = new ROSLIB.Ros({
    url: 'ws://localhost:9090',
  })

  ros.on('connection', () => {
    console.log('Connected to ROS')
    store.setConnected(true)
    resetReconnectionState()
    subscribeToTopics()
  })

  ros.on('error', (error) => {
    console.error('ROS error:', error)
  })

  ros.on('close', () => {
    console.log('Disconnected from ROS')
    store.setConnected(false)
    scheduleReconnect()
  })
}

function subscribeToTopics() {
  if (!ros) return
  const store = useAppStore.getState()

  // World state
  const worldStateTopic = new ROSLIB.Topic({
    ros,
    name: '/world/state',
    messageType: 'warehouser_msgs/msg/WorldState',
  })

  worldStateTopic.subscribe((msg: unknown) => {
    const message = msg as { entities: unknown[]; sim_time: number }
    const entities: Entity[] = message.entities.map((e: unknown) => {
      const entity = e as {
        id: string
        type: number
        x: number
        y: number
        theta?: number
        color?: string
        width?: number
        height?: number
        is_carrying?: boolean
        carried_id?: string
      }
      const typeMap: Record<number, Entity['type']> = {
        0: 'robot',
        1: 'object',
        2: 'wall',
        3: 'zone',
      }
      return {
        id: entity.id,
        type: typeMap[entity.type] || 'object',
        x: entity.x,
        y: entity.y,
        theta: entity.theta,
        color: entity.color,
        width: entity.width,
        height: entity.height,
        isCarrying: entity.is_carrying,
        carriedId: entity.carried_id,
      }
    })
    store.setEntities(entities)
    store.setSimTime(message.sim_time)
  })

  // Lidar debug
  const lidarTopic = new ROSLIB.Topic({
    ros,
    name: '/observations/lidar_debug',
    messageType: 'warehouser_msgs/msg/LidarDebug',
  })

  lidarTopic.subscribe((msg: unknown) => {
    const message = msg as {
      ranges: number[]
      angle_min: number
      angle_max: number
      robot_x: number
      robot_y: number
      robot_theta: number
    }
    store.setLidar(
      message.ranges,
      message.angle_min,
      message.angle_max,
      message.robot_x,
      message.robot_y,
      message.robot_theta
    )
  })

  // Task status
  const taskTopic = new ROSLIB.Topic({
    ros,
    name: '/task/status',
    messageType: 'warehouser_msgs/msg/TaskStatus',
  })

  taskTopic.subscribe((msg: unknown) => {
    const message = msg as { state: string; intent: string }
    store.setTaskStatus(message.state, message.intent)
  })
}

/**
 * Call a ROS service
 *
 * @deprecated Use RosConnection class from './RosConnection' instead.
 * Use rosConnection.callService() for service calls.
 * This function will be removed in a future version.
 */
export function callService(name: string): Promise<boolean> {
  return new Promise((resolve) => {
    if (!ros) {
      resolve(false)
      return
    }

    const service = new ROSLIB.Service({
      ros,
      name,
      serviceType: 'std_srvs/srv/Trigger',
    })

    service.callService(new ROSLIB.ServiceRequest({}), (response: unknown) => {
      const resp = response as { success: boolean }
      resolve(resp.success)
    })
  })
}

/**
 * Publish a command to the /command/json topic
 *
 * @deprecated Use RosConnection class from './RosConnection' instead.
 * Use rosConnection.publish() for publishing messages.
 * This function will be removed in a future version.
 */
export function publishCommand(target: string) {
  if (!ros) return

  const topic = new ROSLIB.Topic({
    ros,
    name: '/command/json',
    messageType: 'std_msgs/msg/String',
  })

  const message = new ROSLIB.Message({
    data: JSON.stringify({ action: 'pick', target }),
  })

  topic.publish(message)
}

/**
 * Publish a move entity command to the /sim/move_entity topic
 *
 * @deprecated Use RosConnection class from './RosConnection' instead.
 * Use rosConnection.publish() for publishing messages.
 * This function will be removed in a future version.
 */
export function publishMoveEntity(id: string, x: number, y: number) {
  if (!ros) return

  const topic = new ROSLIB.Topic({
    ros,
    name: '/sim/move_entity',
    messageType: 'std_msgs/msg/String',
  })

  const message = new ROSLIB.Message({
    data: JSON.stringify({ id, x, y }),
  })

  topic.publish(message)
}
