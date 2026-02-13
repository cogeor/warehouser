/**
 * ConnectionStatus - Visual indicator for ROS connection state
 *
 * Displays the current connection status with a colored dot and text.
 * Supports click-to-retry when in error state.
 */

import { useRosConnection } from '../hooks/useRosConnection';

// =============================================================================
// Types
// =============================================================================

/** Props for ConnectionStatus component */
interface ConnectionStatusProps {
  /** Whether to show detailed status text (default: true) */
  showDetails?: boolean;
  /** Additional CSS classes to apply to the container */
  className?: string;
}

// =============================================================================
// Component
// =============================================================================

/**
 * Connection status indicator component.
 *
 * Displays a colored dot with status text:
 * - Green: Connected
 * - Red: Disconnected or error (click to retry)
 * - Yellow: Reconnecting with attempt counter (animated pulse)
 *
 * @example
 * ```tsx
 * <ConnectionStatus />
 * <ConnectionStatus showDetails={false} />
 * <ConnectionStatus className="my-4" />
 * ```
 */
export function ConnectionStatus({
  showDetails = true,
  className = '',
}: ConnectionStatusProps): JSX.Element {
  const {
    isConnected,
    connectionError,
    reconnectAttempt,
    maxReconnectAttempts,
    retryConnection,
  } = useRosConnection();

  // Determine current state
  const isReconnecting = !isConnected && reconnectAttempt > 0 && !connectionError;
  const hasError = connectionError !== null;
  const isDisconnected = !isConnected && !isReconnecting && !hasError;

  // Determine colors based on state
  const getDotClasses = (): string => {
    const baseClasses = 'w-2 h-2 rounded-full';

    if (isConnected) {
      return `${baseClasses} bg-green-500`;
    }
    if (isReconnecting) {
      return `${baseClasses} bg-yellow-500 animate-pulse`;
    }
    // Disconnected or error
    return `${baseClasses} bg-red-500`;
  };

  const getTextClasses = (): string => {
    if (isConnected) {
      return 'text-green-400';
    }
    if (isReconnecting) {
      return 'text-yellow-400';
    }
    // Disconnected or error
    return 'text-red-400';
  };

  // Determine status text
  const getStatusText = (): string => {
    if (isConnected) {
      return 'Connected';
    }
    if (isReconnecting) {
      return `Reconnecting (attempt ${reconnectAttempt}/${maxReconnectAttempts})`;
    }
    if (hasError) {
      return connectionError;
    }
    return 'Disconnected';
  };

  // Handle click for retry
  const handleClick = (): void => {
    if (hasError || isDisconnected) {
      retryConnection();
    }
  };

  // Handle keyboard interaction for retry
  const handleKeyDown = (event: React.KeyboardEvent): void => {
    if ((hasError || isDisconnected) && (event.key === 'Enter' || event.key === ' ')) {
      event.preventDefault();
      retryConnection();
    }
  };

  // Determine if clickable
  const isClickable = hasError || isDisconnected;

  const containerClasses = [
    'flex items-center gap-2',
    isClickable ? 'cursor-pointer hover:opacity-80 transition-opacity' : '',
    className,
  ]
    .filter(Boolean)
    .join(' ');

  return (
    <div
      className={containerClasses}
      onClick={isClickable ? handleClick : undefined}
      onKeyDown={isClickable ? handleKeyDown : undefined}
      role={isClickable ? 'button' : undefined}
      tabIndex={isClickable ? 0 : undefined}
      title={isClickable ? 'Click to retry connection' : undefined}
    >
      <span className={getDotClasses()} aria-hidden="true" />
      {showDetails && (
        <span className={`text-sm ${getTextClasses()}`}>{getStatusText()}</span>
      )}
    </div>
  );
}

// =============================================================================
// Exports
// =============================================================================

export type { ConnectionStatusProps };
