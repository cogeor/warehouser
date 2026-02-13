/**
 * RosConnectionProvider - React context provider for ROS connection management
 *
 * Provides a React context that wraps RosConnection for use throughout the
 * application. Handles connection lifecycle, state updates, and cleanup.
 */

import { createContext, useContext, useEffect, useState, useCallback, useRef } from 'react';
import type { ReactNode } from 'react';
import type { Ros } from 'roslib';
import { RosConnection } from '../ros/RosConnection';

// =============================================================================
// Types
// =============================================================================

/** Props for RosConnectionProvider component */
interface RosConnectionProviderProps {
  /** Child components that will have access to the ROS connection context */
  children: ReactNode;
  /** Optional WebSocket URL for rosbridge (defaults to config value) */
  url?: string;
}

/** Context value provided by RosConnectionProvider */
interface RosConnectionContextValue {
  /** The underlying ROSLIB.Ros instance, or null if not connected */
  ros: Ros | null;
  /** Whether the connection is currently established */
  isConnected: boolean;
  /** Error message from the last connection error, or null if none */
  connectionError: string | null;
  /** Current reconnection attempt number (0 if connected or not reconnecting) */
  reconnectAttempt: number;
  /** Maximum number of reconnection attempts before giving up */
  maxReconnectAttempts: number;
  /** Manually retry the connection (resets reconnect attempts) */
  retryConnection: () => void;
}

// =============================================================================
// Context
// =============================================================================

/**
 * React context for ROS connection state and controls.
 * Use the useRosConnection hook to access this context.
 */
const RosConnectionContext = createContext<RosConnectionContextValue | null>(null);

// =============================================================================
// Provider Component
// =============================================================================

/**
 * Provider component that manages ROS connection lifecycle.
 *
 * Creates a RosConnection instance on mount, connects automatically,
 * and cleans up on unmount. Updates state based on connection events.
 *
 * @example
 * ```tsx
 * import { RosConnectionProvider } from './hooks/useRosConnection';
 *
 * function App() {
 *   return (
 *     <RosConnectionProvider>
 *       <MyComponent />
 *     </RosConnectionProvider>
 *   );
 * }
 * ```
 */
export function RosConnectionProvider({
  children,
  url,
}: RosConnectionProviderProps): JSX.Element {
  // Connection instance ref (stable across renders)
  const connectionRef = useRef<RosConnection | null>(null);

  // Connection state
  const [ros, setRos] = useState<Ros | null>(null);
  const [isConnected, setIsConnected] = useState<boolean>(false);
  const [connectionError, setConnectionError] = useState<string | null>(null);
  const [reconnectAttempt, setReconnectAttempt] = useState<number>(0);
  const [maxReconnectAttempts, setMaxReconnectAttempts] = useState<number>(0);

  // Initialize connection on mount
  useEffect(() => {
    // Create connection instance
    const connection = url !== undefined
      ? new RosConnection(url)
      : new RosConnection();
    connectionRef.current = connection;

    // Set max reconnect attempts from connection config
    setMaxReconnectAttempts(connection.maxReconnectAttempts);

    // Event handlers
    const handleConnected = (): void => {
      setRos(connection.getRos());
      setIsConnected(true);
      setConnectionError(null);
      setReconnectAttempt(0);
    };

    const handleDisconnected = (): void => {
      setIsConnected(false);
      setReconnectAttempt(connection.currentReconnectAttempt);
    };

    const handleError = (error: Error): void => {
      setConnectionError(error.message);
      setReconnectAttempt(connection.currentReconnectAttempt);
    };

    // Register event listeners
    connection.on('connected', handleConnected);
    connection.on('disconnected', handleDisconnected);
    connection.on('error', handleError);

    // Connect automatically
    connection.connect();

    // Cleanup on unmount
    return () => {
      connection.off('connected', handleConnected);
      connection.off('disconnected', handleDisconnected);
      connection.off('error', handleError);
      connection.disconnect();
      connectionRef.current = null;
    };
  }, [url]);

  // Manual retry connection function
  const retryConnection = useCallback((): void => {
    const connection = connectionRef.current;
    if (connection) {
      setConnectionError(null);
      setReconnectAttempt(0);
      connection.disconnect();
      connection.connect();
    }
  }, []);

  // Context value
  const contextValue: RosConnectionContextValue = {
    ros,
    isConnected,
    connectionError,
    reconnectAttempt,
    maxReconnectAttempts,
    retryConnection,
  };

  return (
    <RosConnectionContext.Provider value={contextValue}>
      {children}
    </RosConnectionContext.Provider>
  );
}

// =============================================================================
// Hook
// =============================================================================

/**
 * Hook to access the ROS connection context.
 *
 * Must be used within a RosConnectionProvider. Throws an error if used
 * outside the provider.
 *
 * @returns The ROS connection context value
 * @throws Error if used outside of RosConnectionProvider
 *
 * @example
 * ```tsx
 * import { useRosConnection } from './hooks/useRosConnection';
 *
 * function StatusIndicator() {
 *   const { isConnected, connectionError, retryConnection } = useRosConnection();
 *
 *   if (connectionError) {
 *     return (
 *       <div>
 *         <span>Error: {connectionError}</span>
 *         <button onClick={retryConnection}>Retry</button>
 *       </div>
 *     );
 *   }
 *
 *   return <span>{isConnected ? 'Connected' : 'Connecting...'}</span>;
 * }
 * ```
 */
export function useRosConnection(): RosConnectionContextValue {
  const context = useContext(RosConnectionContext);

  if (context === null) {
    throw new Error(
      'useRosConnection must be used within a RosConnectionProvider'
    );
  }

  return context;
}

// =============================================================================
// Exports
// =============================================================================

export type { RosConnectionProviderProps, RosConnectionContextValue };
