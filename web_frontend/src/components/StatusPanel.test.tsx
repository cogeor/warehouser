import { describe, it, expect, beforeEach, vi } from 'vitest'
import { render, screen } from '@testing-library/react'
import { StatusPanel } from './StatusPanel'
import { useAppStore } from '../store/appStore'

// Mock the useRosConnection hook
vi.mock('../hooks/useRosConnection', () => ({
  useRosConnection: () => ({
    ros: null,
    isConnected: true,
    connectionError: null,
    reconnectAttempt: 0,
    maxReconnectAttempts: 10,
    retryConnection: vi.fn(),
  }),
}))

describe('StatusPanel', () => {
  beforeEach(() => {
    useAppStore.setState({
      taskState: 'IDLE',
      taskIntent: '',
      simTime: 0,
      entities: [],
    })
  })

  it('renders status heading', () => {
    render(<StatusPanel />)
    expect(screen.getByText('Status')).toBeInTheDocument()
  })

  it('displays task state', () => {
    useAppStore.setState({ taskState: 'NAVIGATING_TO_PICK' })
    render(<StatusPanel />)
    expect(screen.getByText('NAVIGATING_TO_PICK')).toBeInTheDocument()
  })

  it('displays task intent when present', () => {
    useAppStore.setState({ taskIntent: 'pick_and_place' })
    render(<StatusPanel />)
    expect(screen.getByText('pick_and_place')).toBeInTheDocument()
  })

  it('displays simulation time', () => {
    useAppStore.setState({ simTime: 45.67 })
    render(<StatusPanel />)
    expect(screen.getByText('45.67s')).toBeInTheDocument()
  })

  it('displays robot position when robot exists', () => {
    useAppStore.setState({
      entities: [{ id: 'robot', type: 'robot', x: 2.5, y: 3.75 }],
    })
    render(<StatusPanel />)
    expect(screen.getByText('(2.50, 3.75)')).toBeInTheDocument()
  })

  it('displays carrying status when robot is carrying', () => {
    useAppStore.setState({
      entities: [{ id: 'robot', type: 'robot', x: 0, y: 0, isCarrying: true }],
    })
    render(<StatusPanel />)
    expect(screen.getByText('Yes')).toBeInTheDocument()
  })

  it('displays not carrying when robot is not carrying', () => {
    useAppStore.setState({
      entities: [{ id: 'robot', type: 'robot', x: 0, y: 0, isCarrying: false }],
    })
    render(<StatusPanel />)
    expect(screen.getByText('No')).toBeInTheDocument()
  })
})
