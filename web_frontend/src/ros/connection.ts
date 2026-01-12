import ROSLIB from 'roslib'
import { useAppStore, Entity } from '../store/appStore'

let ros: ROSLIB.Ros | null = null

export function initRosConnection() {
  const store = useAppStore.getState()

  ros = new ROSLIB.Ros({
    url: 'ws://localhost:9090',
  })

  ros.on('connection', () => {
    console.log('Connected to ROS')
    store.setConnected(true)
    subscribeToTopics()
  })

  ros.on('error', (error) => {
    console.error('ROS error:', error)
  })

  ros.on('close', () => {
    console.log('Disconnected from ROS')
    store.setConnected(false)

    // Reconnect after 2 seconds
    setTimeout(() => {
      if (ros) {
        ros.connect('ws://localhost:9090')
      }
    }, 2000)
  })
}

function subscribeToTopics() {
  if (!ros) return
  const store = useAppStore.getState()

  // World state
  const worldStateTopic = new ROSLIB.Topic({
    ros,
    name: '/world/state',
    messageType: 'warehouser_msgs/WorldState',
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
    messageType: 'warehouser_msgs/LidarDebug',
  })

  lidarTopic.subscribe((msg: unknown) => {
    const message = msg as { ranges: number[]; angle_min: number; angle_max: number }
    store.setLidar(message.ranges, message.angle_min, message.angle_max)
  })

  // Task status
  const taskTopic = new ROSLIB.Topic({
    ros,
    name: '/task/status',
    messageType: 'warehouser_msgs/TaskStatus',
  })

  taskTopic.subscribe((msg: unknown) => {
    const message = msg as { state: string; intent: string }
    store.setTaskStatus(message.state, message.intent)
  })
}

export function callService(name: string): Promise<boolean> {
  return new Promise((resolve) => {
    if (!ros) {
      resolve(false)
      return
    }

    const service = new ROSLIB.Service({
      ros,
      name,
      serviceType: 'std_srvs/Trigger',
    })

    service.callService(new ROSLIB.ServiceRequest({}), (response: unknown) => {
      const resp = response as { success: boolean }
      resolve(resp.success)
    })
  })
}

export function publishCommand(target: string) {
  if (!ros) return

  const topic = new ROSLIB.Topic({
    ros,
    name: '/command/json',
    messageType: 'std_msgs/String',
  })

  const message = new ROSLIB.Message({
    data: JSON.stringify({ action: 'pick', target }),
  })

  topic.publish(message)
}

export function publishMoveEntity(id: string, x: number, y: number) {
  if (!ros) return

  const topic = new ROSLIB.Topic({
    ros,
    name: '/sim/move_entity',
    messageType: 'std_msgs/String',
  })

  const message = new ROSLIB.Message({
    data: JSON.stringify({ id, x, y }),
  })

  topic.publish(message)
}
