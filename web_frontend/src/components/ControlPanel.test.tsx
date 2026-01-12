import { describe, it, expect, vi, beforeEach } from 'vitest'
import { render, screen, fireEvent, waitFor } from '@testing-library/react'
import { ControlPanel } from './ControlPanel'
import { useAppStore } from '../store/appStore'

// Mock the ROS connection
vi.mock('../ros/connection', () => ({
  callService: vi.fn().mockResolvedValue(true),
}))

import { callService } from '../ros/connection'

describe('ControlPanel', () => {
  beforeEach(() => {
    vi.clearAllMocks()
    useAppStore.setState({ simRunning: false })
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
      expect(callService).toHaveBeenCalledWith('/sim/start')
    })
  })

  it('calls pause service on pause click', async () => {
    useAppStore.setState({ simRunning: true })
    render(<ControlPanel />)
    fireEvent.click(screen.getByText('Pause'))
    await waitFor(() => {
      expect(callService).toHaveBeenCalledWith('/sim/pause')
    })
  })

  it('calls reset service on reset click', async () => {
    render(<ControlPanel />)
    fireEvent.click(screen.getByText('Reset'))
    await waitFor(() => {
      expect(callService).toHaveBeenCalledWith('/sim/reset')
    })
  })
})
