import { RosConnectionProvider } from './hooks/useRosConnection'
import { RosDataBridge } from './components/RosDataBridge'
import { LayoutSplit } from './layouts/LayoutSplit'

function App() {
  return (
    <RosConnectionProvider>
      <RosDataBridge />
      <LayoutSplit />
    </RosConnectionProvider>
  )
}

export default App
