import { RosConnectionProvider } from './hooks/useRosConnection'
import { RosDataBridge } from './components/RosDataBridge'
import { CurrentLayout } from './layouts'
import { DesignSwitcher } from './components/ui/DesignSwitcher'

function App() {
  return (
    <RosConnectionProvider>
      <RosDataBridge />
      <CurrentLayout />
      <DesignSwitcher />
    </RosConnectionProvider>
  )
}

export default App
