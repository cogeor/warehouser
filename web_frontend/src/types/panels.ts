/**
 * Panel system type definitions and registry
 * Provides interfaces for configurable UI panels and a singleton registry
 */

import type { ComponentType } from 'react';

// ============================================================================
// Panel Configuration
// ============================================================================

/**
 * Configuration for a panel component
 */
export interface PanelConfig {
  /** Unique identifier for the panel */
  id: string;

  /** Display title for the panel */
  title: string;

  /** Optional icon name (e.g., for icon library lookup) */
  icon?: string;

  /** Whether panel is visible by default */
  defaultVisible?: boolean;

  /** Minimum width in pixels */
  minWidth?: number;

  /** Minimum height in pixels */
  minHeight?: number;
}

// ============================================================================
// Panel Props
// ============================================================================

/**
 * Props passed to panel components
 */
export interface PanelProps {
  /** Whether the panel is currently collapsed */
  isCollapsed?: boolean;

  /** Callback to toggle collapse state */
  onToggleCollapse?: () => void;
}

// ============================================================================
// Panel Component
// ============================================================================

/**
 * A registered panel with its configuration and React component
 */
export interface PanelComponent {
  /** Panel configuration */
  config: PanelConfig;

  /** React component to render the panel content */
  component: ComponentType<PanelProps>;
}

// ============================================================================
// Panel Registry
// ============================================================================

/**
 * Registry for managing panel components
 *
 * Provides methods to register, unregister, and retrieve panels by ID.
 * Use the exported singleton `panelRegistry` for application-wide panel management.
 */
export class PanelRegistry {
  private panels: Map<string, PanelComponent> = new Map();

  /**
   * Register a panel component
   * @param panel - The panel component to register
   */
  register(panel: PanelComponent): void {
    this.panels.set(panel.config.id, panel);
  }

  /**
   * Unregister a panel by ID
   * @param id - The panel ID to unregister
   */
  unregister(id: string): void {
    this.panels.delete(id);
  }

  /**
   * Get a panel by ID
   * @param id - The panel ID to retrieve
   * @returns The panel component or undefined if not found
   */
  get(id: string): PanelComponent | undefined {
    return this.panels.get(id);
  }

  /**
   * Get all registered panels
   * @returns Array of all registered panel components
   */
  getAll(): PanelComponent[] {
    return Array.from(this.panels.values());
  }

  /**
   * Check if a panel is registered
   * @param id - The panel ID to check
   * @returns True if the panel is registered
   */
  has(id: string): boolean {
    return this.panels.has(id);
  }
}

// ============================================================================
// Singleton Instance
// ============================================================================

/**
 * Singleton panel registry instance for application-wide use
 */
export const panelRegistry = new PanelRegistry();
