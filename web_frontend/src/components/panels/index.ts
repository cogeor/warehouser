/**
 * Panel barrel exports and registration
 * This module exports all panel components and registers them with the panel registry.
 */

// Export all panel components and configs
export { StatusPanel, statusPanelConfig } from './StatusPanel'
export { ControlPanel, controlPanelConfig } from './ControlPanel'
export { ObjectivePanel, objectivePanelConfig } from './ObjectivePanel'
export { MapPanel, mapPanelConfig } from './MapPanel'

import { panelRegistry } from '../../types/panels'

import { StatusPanel, statusPanelConfig } from './StatusPanel'
import { ControlPanel, controlPanelConfig } from './ControlPanel'
import { ObjectivePanel, objectivePanelConfig } from './ObjectivePanel'
import { MapPanel, mapPanelConfig } from './MapPanel'

// Register all panels
panelRegistry.register({
  config: statusPanelConfig,
  component: StatusPanel,
})
panelRegistry.register({
  config: controlPanelConfig,
  component: ControlPanel,
})
panelRegistry.register({
  config: objectivePanelConfig,
  component: ObjectivePanel,
})
panelRegistry.register({
  config: mapPanelConfig,
  component: MapPanel,
})

// Convenience array of all panel configs
export const allPanelConfigs = [
  statusPanelConfig,
  controlPanelConfig,
  objectivePanelConfig,
  mapPanelConfig,
]
