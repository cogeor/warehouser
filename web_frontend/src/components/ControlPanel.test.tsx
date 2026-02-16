import { describe, it, expect, vi, beforeEach } from 'vitest'
import { render, screen, fireEvent, waitFor } from '@testing-library/react'
import type { ReactNode } from 'react'
import { ControlPanel } from './ControlPanel'
import { useAppStore } from '../store/appStore'

// Mock trigger service calls
const mockStartSim = vi.fn().mockResolvedValue(true)
const mockPauseSim = vi.fn().mockResolvedValue(true)
const mockResetSim = vi.fn().mockResolvedValue(true)
const mockPublishJson = vi.fn()
const mockPublishPolicyEnable = vi.fn()

// Mock the ROS service hooks
vi.mock('../hooks/useRosService', () => ({
  useTriggerService: (serviceName: string) => {
    if (serviceName === '/sim/start') return mockStartSim
    if (serviceName === '/sim/pause') return mockPauseSim
    if (serviceName === '/sim/reset') return mockResetSim
    return vi.fn().mockResolvedValue(false)
  },
  useRosPublisher: (topic: string) => {
    if (topic === '/inference/enable') return mockPublishPolicyEnable
    return mockPublishJson
  },
}))

// Mock RosConnectionProvider
vi.mock('../hooks/useRosConnection', () => ({
  useRosConnection: () => ({
    ros: {},
    isConnected: true,
    connectionError: null,
    reconnectAttempt: 0,
    maxReconnectAttempts: 10,
    retryConnection: vi.fn(),
  }),
  RosConnectionProvider: ({ children }: { children: ReactNode }) => children,
}))

describe('ControlPanel', () => {
  beforeEach(() => {
    vi.clearAllMocks()
    useAppStore.setState({ simRunning: false, demoActive: false, policyEnabled: false })
  })

  it('renders start button when not running', () => {
    render(<ControlPanel />)
    expect(screen.getByText('Start')).toBeInTheDocument()
    expect(screen.queryByText('Pause')).not.toBeInTheDocument()
  })

  it('renders pause button when running', () => {
    useAppStore.setState({ simRunning: true })
    render(<ControlPanel />)
    expect(screen.getByText('Pause')).toBeInTheDocument()
    expect(screen.queryByText('Start')).not.toBeInTheDocument()
  })

  it('always renders reset button', () => {
    render(<ControlPanel />)
    expect(screen.getByText('Reset')).toBeInTheDocument()
  })

  it('calls start service on start click', async () => {
    render(<ControlPanel />)
    fireEvent.click(screen.getByText('Start'))
    await waitFor(() => {
      expect(mockStartSim).toHaveBeenCalled()
    })
  })

  it('calls pause service on pause click', async () => {
    useAppStore.setState({ simRunning: true })
    render(<ControlPanel />)
    fireEvent.click(screen.getByText('Pause'))
    await waitFor(() => {
      expect(mockPauseSim).toHaveBeenCalled()
    })
  })

  it('calls reset service on reset click', async () => {
    render(<ControlPanel />)
    fireEvent.click(screen.getByText('Reset'))
    await waitFor(() => {
      expect(mockResetSim).toHaveBeenCalled()
    })
  })

  describe('policy toggle', () => {
    it('renders policy toggle', () => {
      render(<ControlPanel />)
      expect(screen.getByText('Policy Inference')).toBeInTheDocument()
    })

    it('publishes to /inference/enable when toggled on', async () => {
      render(<ControlPanel />)
      const toggle = screen.getByText('Policy Inference').parentElement?.querySelector('button')
      expect(toggle).toBeTruthy()
      fireEvent.click(toggle!)
      await waitFor(() => {
        expect(mockPublishPolicyEnable).toHaveBeenCalledWith({ data: true })
      })
    })

    it('publishes to /inference/enable when toggled off', async () => {
      useAppStore.setState({ policyEnabled: true })
      render(<ControlPanel />)
      const toggle = screen.getByText('Policy Inference').parentElement?.querySelector('button')
      expect(toggle).toBeTruthy()
      fireEvent.click(toggle!)
      await waitFor(() => {
        expect(mockPublishPolicyEnable).toHaveBeenCalledWith({ data: false })
      })
    })

    it('updates store state when toggled', async () => {
      render(<ControlPanel />)
      expect(useAppStore.getState().policyEnabled).toBe(false)
      const toggle = screen.getByText('Policy Inference').parentElement?.querySelector('button')
      fireEvent.click(toggle!)
      await waitFor(() => {
        expect(useAppStore.getState().policyEnabled).toBe(true)
      })
    })

    it('shows status text when policy is enabled', () => {
      useAppStore.setState({ policyEnabled: true })
      render(<ControlPanel />)
      expect(screen.getByText('Policy running...')).toBeInTheDocument()
    })
  })
})
