/**
 * RosConnection - WebSocket connection manager for ROS2 via roslib
 *
 * Provides lifecycle management with:
 * - Automatic reconnection with exponential backoff and jitter
 * - Event emitter pattern for connection state changes
 * - Clean disconnect handling
 */

import ROSLIB from 'roslib'
import { ROS_WS_URL, RECONNECT_CONFIG } from '../config'

// =============================================================================
// Types
// =============================================================================

/** Event types emitted by RosConnection */
export type RosConnectionEvent = 'connected' | 'disconnected' | 'error'

/** Event listener callback signatures */
export type RosConnectionEventCallback<T extends RosConnectionEvent> =
  T extends 'connected' ? () => void :
  T extends 'disconnected' ? () => void :
  T extends 'error' ? (error: Error) => void :
  never

/** Internal storage for event listeners */
type EventListeners = {
  connected: Array<() => void>
  disconnected: Array<() => void>
  error: Array<(error: Error) => void>
}

// =============================================================================
// RosConnection Class
// =============================================================================

/**
 * Manages a WebSocket connection to ROS2 via rosbridge.
 *
 * Features:
 * - Configurable WebSocket URL (defaults to ROS_WS_URL from config)
 * - Automatic reconnection with exponential backoff
 * - Event emitters for 'connected', 'disconnected', 'error'
 * - Clean lifecycle management via connect() and disconnect()
 *
 * @example
 * ```typescript
 * const connection = new RosConnection()
 *
 * connection.on('connected', () => {
 *   console.log('Connected!')
 *   const ros = connection.getRos()
 *   // Subscribe to topics using ros instance
 * })
 *
 * connection.on('disconnected', () => {
 *   console.log('Disconnected')
 * })
 *
 * connection.on('error', (error) => {
 *   console.error('Connection error:', error)
 * })
 *
 * connection.connect()
 *
 * // Later...
 * connection.disconnect()
 * ```
 */
export class RosConnection {
  private readonly url: string
  private ros: ROSLIB.Ros | null = null
  private reconnectTimeoutId: ReturnType<typeof setTimeout> | null = null
  private reconnectAttempt: number = 0
  private intentionalDisconnect: boolean = false

  private readonly listeners: EventListeners = {
    connected: [],
    disconnected: [],
    error: [],
  }

  /**
   * Create a new RosConnection instance.
   *
   * @param url - WebSocket URL for rosbridge (defaults to ROS_WS_URL from config)
   */
  constructor(url: string = ROS_WS_URL) {
    this.url = url
  }

  // ===========================================================================
  // Public API
  // ===========================================================================

  /**
   * Whether the connection is currently established.
   */
  get isConnected(): boolean {
    return this.ros?.isConnected ?? false
  }

  /**
   * Current reconnection attempt number (0 if connected or not reconnecting).
   */
  get currentReconnectAttempt(): number {
    return this.reconnectAttempt
  }

  /**
   * Maximum number of reconnection attempts before giving up.
   */
  get maxReconnectAttempts(): number {
    return RECONNECT_CONFIG.maxAttempts
  }

  /**
   * Get the underlying ROSLIB.Ros instance.
   *
   * @returns The Ros instance, or null if not connected
   */
  getRos(): ROSLIB.Ros | null {
    return this.ros
  }

  /**
   * Connect to the ROS bridge WebSocket server.
   *
   * Creates a new ROSLIB.Ros instance and initiates the connection.
   * Will trigger 'connected' event on success, or schedule reconnection
   * attempts on failure.
   */
  connect(): void {
    // Reset state for fresh connection
    this.intentionalDisconnect = false
    this.reconnectAttempt = 0
    this.clearReconnectTimeout()

    // Create new Ros instance if needed
    if (!this.ros) {
      this.ros = new ROSLIB.Ros({ url: this.url })
      this.setupEventHandlers()
    }

    // Initiate connection
    this.ros.connect(this.url)
  }

  /**
   * Disconnect from the ROS bridge.
   *
   * Cleanly closes the WebSocket connection and cancels any pending
   * reconnection attempts. Will trigger 'disconnected' event.
   */
  disconnect(): void {
    this.intentionalDisconnect = true
    this.clearReconnectTimeout()
    this.reconnectAttempt = 0

    if (this.ros) {
      this.ros.close()
      this.ros = null
    }
  }

  /**
   * Register an event listener.
   *
   * @param event - Event type: 'connected', 'disconnected', or 'error'
   * @param callback - Function to call when event occurs
   */
  on<T extends RosConnectionEvent>(
    event: T,
    callback: RosConnectionEventCallback<T>
  ): void {
    if (event === 'connected') {
      this.listeners.connected.push(callback as () => void)
    } else if (event === 'disconnected') {
      this.listeners.disconnected.push(callback as () => void)
    } else if (event === 'error') {
      this.listeners.error.push(callback as (error: Error) => void)
    }
  }

  /**
   * Remove an event listener.
   *
   * @param event - Event type: 'connected', 'disconnected', or 'error'
   * @param callback - The callback to remove
   */
  off<T extends RosConnectionEvent>(
    event: T,
    callback: RosConnectionEventCallback<T>
  ): void {
    if (event === 'connected') {
      const index = this.listeners.connected.indexOf(callback as () => void)
      if (index !== -1) {
        this.listeners.connected.splice(index, 1)
      }
    } else if (event === 'disconnected') {
      const index = this.listeners.disconnected.indexOf(callback as () => void)
      if (index !== -1) {
        this.listeners.disconnected.splice(index, 1)
      }
    } else if (event === 'error') {
      const index = this.listeners.error.indexOf(callback as (error: Error) => void)
      if (index !== -1) {
        this.listeners.error.splice(index, 1)
      }
    }
  }

  /**
   * Remove all event listeners for a specific event type, or all events.
   *
   * @param event - Optional event type to clear. If omitted, clears all listeners.
   */
  removeAllListeners(event?: RosConnectionEvent): void {
    if (event === undefined) {
      this.listeners.connected = []
      this.listeners.disconnected = []
      this.listeners.error = []
    } else if (event === 'connected') {
      this.listeners.connected = []
    } else if (event === 'disconnected') {
      this.listeners.disconnected = []
    } else if (event === 'error') {
      this.listeners.error = []
    }
  }

  // ===========================================================================
  // Private Methods
  // ===========================================================================

  /**
   * Set up ROSLIB event handlers for connection, error, and close events.
   */
  private setupEventHandlers(): void {
    if (!this.ros) return

    this.ros.on('connection', () => {
      this.reconnectAttempt = 0
      this.emitEvent('connected')
    })

    this.ros.on('error', (error: Error) => {
      this.emitEvent('error', error)
    })

    this.ros.on('close', () => {
      this.emitEvent('disconnected')

      // Schedule reconnection if this wasn't intentional
      if (!this.intentionalDisconnect) {
        this.scheduleReconnect()
      }
    })
  }

  /**
   * Emit an event to all registered listeners.
   */
  private emitEvent(event: 'connected'): void
  private emitEvent(event: 'disconnected'): void
  private emitEvent(event: 'error', error: Error): void
  private emitEvent(event: RosConnectionEvent, error?: Error): void {
    if (event === 'connected') {
      for (const callback of this.listeners.connected) {
        callback()
      }
    } else if (event === 'disconnected') {
      for (const callback of this.listeners.disconnected) {
        callback()
      }
    } else if (event === 'error' && error) {
      for (const callback of this.listeners.error) {
        callback(error)
      }
    }
  }

  /**
   * Calculate exponential backoff delay with jitter.
   *
   * @param attempt - Current attempt number (0-indexed)
   * @returns Delay in milliseconds
   */
  private calculateBackoffDelay(attempt: number): number {
    // Exponential backoff: baseDelay * factor^attempt
    const exponentialDelay = RECONNECT_CONFIG.baseDelay *
      Math.pow(RECONNECT_CONFIG.factor, attempt)

    // Cap at max delay
    const cappedDelay = Math.min(exponentialDelay, RECONNECT_CONFIG.maxDelay)

    // Add jitter: +/- configured percentage
    const jitterRange = cappedDelay * RECONNECT_CONFIG.jitter
    const jitter = (Math.random() * 2 - 1) * jitterRange

    return Math.round(cappedDelay + jitter)
  }

  /**
   * Schedule a reconnection attempt with exponential backoff.
   */
  private scheduleReconnect(): void {
    // Check if max attempts reached
    if (this.reconnectAttempt >= RECONNECT_CONFIG.maxAttempts) {
      const error = new Error(
        `Connection failed after ${RECONNECT_CONFIG.maxAttempts} attempts`
      )
      this.emitEvent('error', error)
      return
    }

    const delay = this.calculateBackoffDelay(this.reconnectAttempt)

    // Clear any existing timeout
    this.clearReconnectTimeout()

    this.reconnectTimeoutId = setTimeout(() => {
      if (this.ros && !this.intentionalDisconnect) {
        this.reconnectAttempt++
        this.ros.connect(this.url)
      }
    }, delay)
  }

  /**
   * Clear any pending reconnection timeout.
   */
  private clearReconnectTimeout(): void {
    if (this.reconnectTimeoutId !== null) {
      clearTimeout(this.reconnectTimeoutId)
      this.reconnectTimeoutId = null
    }
  }
}

// =============================================================================
// Exports
// =============================================================================

export default RosConnection
