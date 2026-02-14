import { useAppStore } from '../../store/appStore'

interface ConnectionDotProps {
  showLabel?: boolean
  className?: string
}

export function ConnectionDot({ showLabel = false, className = '' }: ConnectionDotProps) {
  const connected = useAppStore((s) => s.connected)
  const reconnectAttempt = useAppStore((s) => s.reconnectAttempt)

  const isReconnecting = !connected && reconnectAttempt > 0

  return (
    <div className={`flex items-center gap-2 ${className}`}>
      <div
        className={`w-2 h-2 rounded-full ${
          connected
            ? 'bg-green-500'
            : isReconnecting
            ? 'bg-yellow-500 animate-pulse'
            : 'bg-red-500'
        }`}
      />
      {showLabel && (
        <span className="text-xs text-gray-500">
          {connected ? 'Connected' : isReconnecting ? 'Reconnecting...' : 'Disconnected'}
        </span>
      )}
    </div>
  )
}
