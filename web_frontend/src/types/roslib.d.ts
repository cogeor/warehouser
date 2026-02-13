/**
 * Type declarations for roslib
 *
 * This file provides TypeScript type declarations for the roslib npm package,
 * which does not include official @types/roslib definitions.
 *
 * Only types used by this project are declared.
 */

declare module 'roslib' {
  export interface RosOptions {
    url: string
  }

  export type RosEventType = 'connection' | 'error' | 'close'

  export class Ros {
    constructor(options: RosOptions)

    /**
     * Register an event listener
     * @param eventType - 'connection', 'error', or 'close'
     * @param callback - Handler for the event
     */
    on(eventType: 'connection', callback: () => void): void
    on(eventType: 'error', callback: (error: Error) => void): void
    on(eventType: 'close', callback: () => void): void

    /**
     * Connect to the ROS bridge WebSocket server
     * @param url - WebSocket URL (e.g., 'ws://localhost:9090')
     */
    connect(url: string): void

    /**
     * Close the connection to the ROS bridge
     */
    close(): void

    /**
     * Check if currently connected
     */
    isConnected: boolean
  }

  export interface TopicOptions {
    ros: Ros
    name: string
    messageType: string
  }

  export class Topic<T = unknown> {
    constructor(options: TopicOptions)

    /**
     * Subscribe to this topic
     * @param callback - Called when a message is received
     */
    subscribe(callback: (message: T) => void): void

    /**
     * Unsubscribe from this topic
     */
    unsubscribe(): void

    /**
     * Publish a message to this topic
     * @param message - The message to publish
     */
    publish(message: Message): void
  }

  export interface ServiceOptions {
    ros: Ros
    name: string
    serviceType: string
  }

  export class Service<TRequest = unknown, TResponse = unknown> {
    constructor(options: ServiceOptions)

    /**
     * Call this service
     * @param request - The service request
     * @param callback - Called with the service response
     */
    callService(
      request: ServiceRequest<TRequest>,
      callback: (response: TResponse) => void
    ): void
  }

  export class ServiceRequest<T = Record<string, unknown>> {
    constructor(data: T)
  }

  export class Message<T = Record<string, unknown>> {
    constructor(data: T)
  }

  const ROSLIB: {
    Ros: typeof Ros
    Topic: typeof Topic
    Service: typeof Service
    ServiceRequest: typeof ServiceRequest
    Message: typeof Message
  }

  export default ROSLIB
}

/**
 * Global namespace declaration for ROSLIB
 * Allows usage like: let ros: ROSLIB.Ros | null = null
 */
declare namespace ROSLIB {
  interface RosOptions {
    url: string
  }

  class Ros {
    constructor(options: RosOptions)
    on(eventType: 'connection', callback: () => void): void
    on(eventType: 'error', callback: (error: Error) => void): void
    on(eventType: 'close', callback: () => void): void
    connect(url: string): void
    close(): void
    isConnected: boolean
  }

  interface TopicOptions {
    ros: Ros
    name: string
    messageType: string
  }

  class Topic<T = unknown> {
    constructor(options: TopicOptions)
    subscribe(callback: (message: T) => void): void
    unsubscribe(): void
    publish(message: Message): void
  }

  interface ServiceOptions {
    ros: Ros
    name: string
    serviceType: string
  }

  class Service<TRequest = unknown, TResponse = unknown> {
    constructor(options: ServiceOptions)
    callService(
      request: ServiceRequest<TRequest>,
      callback: (response: TResponse) => void
    ): void
  }

  class ServiceRequest<T = Record<string, unknown>> {
    constructor(data: T)
  }

  class Message<T = Record<string, unknown>> {
    constructor(data: T)
  }
}
