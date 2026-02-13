/**
 * usePanelConfig - React hook for persisting panel configuration to localStorage
 *
 * Provides a hook for managing and persisting UI panel state (collapsed, visible)
 * and display options (FPS counter, performance metrics) with automatic localStorage
 * persistence.
 */

import { useState, useCallback, useEffect } from 'react';

// =============================================================================
// Constants
// =============================================================================

/** localStorage key for panel configuration */
const STORAGE_KEY = 'warehouser-panel-config';

// =============================================================================
// Types
// =============================================================================

/**
 * State for an individual panel
 */
export interface PanelState {
  /** Whether the panel is collapsed (minimized) */
  isCollapsed: boolean;
  /** Whether the panel is visible */
  isVisible: boolean;
}

/**
 * Complete panel configuration state
 */
export interface PanelConfigState {
  /** Map of panel IDs to their states */
  panels: Record<string, PanelState>;
  /** Whether to show the FPS counter */
  showFps: boolean;
  /** Whether to show performance metrics */
  showPerformance: boolean;
}

/**
 * Return type for the usePanelConfig hook
 */
export interface UsePanelConfigResult {
  /** Current panel configuration state */
  config: PanelConfigState;
  /** Set state for a specific panel */
  setPanelState: (panelId: string, state: Partial<PanelState>) => void;
  /** Toggle a panel's collapsed state */
  togglePanel: (panelId: string) => void;
  /** Set whether to show the FPS counter */
  setShowFps: (show: boolean) => void;
  /** Set whether to show performance metrics */
  setShowPerformance: (show: boolean) => void;
  /** Reset configuration to defaults */
  resetConfig: () => void;
}

// =============================================================================
// Default Configuration
// =============================================================================

/**
 * Default panel configuration state
 */
const defaultConfig: PanelConfigState = {
  panels: {},
  showFps: false,
  showPerformance: false,
};

// =============================================================================
// Helper Functions
// =============================================================================

/**
 * Load configuration from localStorage
 * @returns Parsed configuration or default if not found/invalid
 */
function loadConfig(): PanelConfigState {
  try {
    const stored = localStorage.getItem(STORAGE_KEY);
    if (stored === null) {
      return defaultConfig;
    }
    const parsed: unknown = JSON.parse(stored);
    // Validate the parsed object has the expected shape
    if (isValidConfig(parsed)) {
      return parsed;
    }
    return defaultConfig;
  } catch {
    // If parsing fails, return default config
    return defaultConfig;
  }
}

/**
 * Type guard to validate configuration shape
 * @param value - Value to validate
 * @returns True if value matches PanelConfigState shape
 */
function isValidConfig(value: unknown): value is PanelConfigState {
  if (typeof value !== 'object' || value === null) {
    return false;
  }
  const obj = value as Record<string, unknown>;
  return (
    typeof obj.panels === 'object' &&
    obj.panels !== null &&
    typeof obj.showFps === 'boolean' &&
    typeof obj.showPerformance === 'boolean'
  );
}

/**
 * Save configuration to localStorage
 * @param config - Configuration to save
 */
function saveConfig(config: PanelConfigState): void {
  try {
    localStorage.setItem(STORAGE_KEY, JSON.stringify(config));
  } catch {
    // Silently fail if localStorage is unavailable
    console.warn('Failed to save panel config to localStorage');
  }
}

// =============================================================================
// Hook
// =============================================================================

/**
 * Hook for managing and persisting panel configuration.
 *
 * Loads configuration from localStorage on mount and saves changes automatically.
 * Provides methods to update individual panel states and global display options.
 *
 * @returns Object with config state and update methods
 *
 * @example
 * ```tsx
 * function PanelContainer() {
 *   const {
 *     config,
 *     setPanelState,
 *     togglePanel,
 *     setShowFps,
 *     resetConfig
 *   } = usePanelConfig();
 *
 *   const isPanelVisible = config.panels['sidebar']?.isVisible ?? true;
 *
 *   return (
 *     <div>
 *       <button onClick={() => togglePanel('sidebar')}>Toggle Sidebar</button>
 *       <button onClick={() => setShowFps(!config.showFps)}>Toggle FPS</button>
 *       {isPanelVisible && <Sidebar />}
 *     </div>
 *   );
 * }
 * ```
 */
export function usePanelConfig(): UsePanelConfigResult {
  // Initialize state with lazy loader from localStorage
  const [config, setConfig] = useState<PanelConfigState>(loadConfig);

  // Save to localStorage whenever config changes
  useEffect(() => {
    saveConfig(config);
  }, [config]);

  /**
   * Set state for a specific panel
   */
  const setPanelState = useCallback(
    (panelId: string, state: Partial<PanelState>): void => {
      setConfig((prev) => {
        const currentPanelState = prev.panels[panelId] ?? {
          isCollapsed: false,
          isVisible: true,
        };
        return {
          ...prev,
          panels: {
            ...prev.panels,
            [panelId]: {
              ...currentPanelState,
              ...state,
            },
          },
        };
      });
    },
    []
  );

  /**
   * Toggle a panel's collapsed state
   */
  const togglePanel = useCallback(
    (panelId: string): void => {
      setConfig((prev) => {
        const currentPanelState = prev.panels[panelId] ?? {
          isCollapsed: false,
          isVisible: true,
        };
        return {
          ...prev,
          panels: {
            ...prev.panels,
            [panelId]: {
              ...currentPanelState,
              isCollapsed: !currentPanelState.isCollapsed,
            },
          },
        };
      });
    },
    []
  );

  /**
   * Set whether to show the FPS counter
   */
  const setShowFps = useCallback((show: boolean): void => {
    setConfig((prev) => ({
      ...prev,
      showFps: show,
    }));
  }, []);

  /**
   * Set whether to show performance metrics
   */
  const setShowPerformance = useCallback((show: boolean): void => {
    setConfig((prev) => ({
      ...prev,
      showPerformance: show,
    }));
  }, []);

  /**
   * Reset configuration to defaults
   */
  const resetConfig = useCallback((): void => {
    setConfig(defaultConfig);
  }, []);

  return {
    config,
    setPanelState,
    togglePanel,
    setShowFps,
    setShowPerformance,
    resetConfig,
  };
}
