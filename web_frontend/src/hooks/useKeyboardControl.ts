/**
 * Keyboard control hook for manual robot driving.
 *
 * Arrow keys or WASD to move the robot.
 * Publishes Twist messages directly to /cmd_vel_raw.
 */

import { useEffect, useRef, useCallback } from 'react'
import { useRosConnection } from './useRosConnection'
import ROSLIB from 'roslib'

interface VelocityState {
  linear: number
  angular: number
}

const LINEAR_SPEED = 0.5 // m/s
const ANGULAR_SPEED = 1.5 // rad/s
const PUBLISH_RATE = 20 // Hz

export function useKeyboardControl() {
  const { ros, isConnected } = useRosConnection()
  const velocityRef = useRef<VelocityState>({ linear: 0, angular: 0 })
  const keysPressed = useRef<Set<string>>(new Set())
  const publisherRef = useRef<ROSLIB.Topic | null>(null)
  const intervalRef = useRef<ReturnType<typeof setInterval> | null>(null)

  // Create publisher when connected
  useEffect(() => {
    if (!ros || !isConnected) {
      publisherRef.current = null
      console.log('[KeyboardControl] Not connected, skipping publisher setup')
      return
    }

    console.log('[KeyboardControl] Creating publisher for /cmd_vel_raw')
    publisherRef.current = new ROSLIB.Topic({
      ros: ros as ROSLIB.Ros,
      name: '/cmd_vel_raw',
      messageType: 'geometry_msgs/msg/Twist',
    })

    return () => {
      publisherRef.current = null
    }
  }, [ros, isConnected])

  // Update velocity based on pressed keys
  const updateVelocity = useCallback(() => {
    const keys = keysPressed.current
    let linear = 0
    let angular = 0

    // Forward/backward
    if (keys.has('ArrowUp') || keys.has('w') || keys.has('W')) {
      linear += LINEAR_SPEED
    }
    if (keys.has('ArrowDown') || keys.has('s') || keys.has('S')) {
      linear -= LINEAR_SPEED
    }

    // Left/right rotation
    if (keys.has('ArrowLeft') || keys.has('a') || keys.has('A')) {
      angular += ANGULAR_SPEED
    }
    if (keys.has('ArrowRight') || keys.has('d') || keys.has('D')) {
      angular -= ANGULAR_SPEED
    }

    velocityRef.current = { linear, angular }
  }, [])

  // Publish velocity command
  const publishVelocity = useCallback(() => {
    if (!publisherRef.current) return

    const { linear, angular } = velocityRef.current

    // Only log when we have non-zero velocity
    if (linear !== 0 || angular !== 0) {
      console.log(`[KeyboardControl] Publishing: linear=${linear.toFixed(2)}, angular=${angular.toFixed(2)}`)
    }

    const message = new ROSLIB.Message({
      linear: { x: linear, y: 0, z: 0 },
      angular: { x: 0, y: 0, z: angular },
    })

    publisherRef.current.publish(message)
  }, [])

  // Handle key events
  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      // Ignore if typing in an input
      if (e.target instanceof HTMLInputElement || e.target instanceof HTMLTextAreaElement) {
        return
      }

      const key = e.key
      if (['ArrowUp', 'ArrowDown', 'ArrowLeft', 'ArrowRight', 'w', 'W', 'a', 'A', 's', 'S', 'd', 'D'].includes(key)) {
        e.preventDefault()
        keysPressed.current.add(key)
        updateVelocity()
      }
    }

    const handleKeyUp = (e: KeyboardEvent) => {
      keysPressed.current.delete(e.key)
      updateVelocity()
    }

    window.addEventListener('keydown', handleKeyDown)
    window.addEventListener('keyup', handleKeyUp)

    return () => {
      window.removeEventListener('keydown', handleKeyDown)
      window.removeEventListener('keyup', handleKeyUp)
    }
  }, [updateVelocity])

  // Start/stop publishing loop
  useEffect(() => {
    if (!isConnected) {
      if (intervalRef.current) {
        clearInterval(intervalRef.current)
        intervalRef.current = null
      }
      return
    }

    // Publish at fixed rate
    intervalRef.current = setInterval(publishVelocity, 1000 / PUBLISH_RATE)

    return () => {
      if (intervalRef.current) {
        clearInterval(intervalRef.current)
        intervalRef.current = null
      }
    }
  }, [isConnected, publishVelocity])

  return {
    velocity: velocityRef.current,
    isActive: keysPressed.current.size > 0,
  }
}
