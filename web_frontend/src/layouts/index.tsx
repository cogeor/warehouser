import { useDesignStore, DesignId } from '../store/designStore'
import { LayoutMinimal } from './LayoutMinimal'
import { LayoutCAD } from './LayoutCAD'
import { LayoutSplit } from './LayoutSplit'
import { LayoutFloating } from './LayoutFloating'
import { LayoutImmersive } from './LayoutImmersive'

const LAYOUT_COMPONENTS: Record<DesignId, React.ComponentType> = {
  1: LayoutMinimal,
  2: LayoutCAD,
  3: LayoutSplit,
  4: LayoutFloating,
  5: LayoutImmersive,
}

export function CurrentLayout() {
  const currentDesign = useDesignStore((s) => s.currentDesign)
  const LayoutComponent = LAYOUT_COMPONENTS[currentDesign]
  return <LayoutComponent />
}

export { LayoutMinimal, LayoutCAD, LayoutSplit, LayoutFloating, LayoutImmersive }
