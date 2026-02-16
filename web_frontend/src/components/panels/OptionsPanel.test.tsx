import { describe, it, expect, beforeEach } from 'vitest'
import { render, screen, fireEvent } from '@testing-library/react'
import { OptionsPanel } from './OptionsPanel'
import { useAppStore } from '../../store/appStore'

describe('OptionsPanel', () => {
  beforeEach(() => {
    useAppStore.setState({
      traceEnabled: false,
      trajectoryHistory: [],
    })
  })

  it('renders trajectory checkbox', () => {
    render(<OptionsPanel />)
    expect(screen.getByLabelText('Show trajectory')).toBeInTheDocument()
  })

  it('checkbox reflects traceEnabled state when false', () => {
    useAppStore.setState({ traceEnabled: false })
    render(<OptionsPanel />)
    const checkbox = screen.getByLabelText('Show trajectory') as HTMLInputElement
    expect(checkbox.checked).toBe(false)
  })

  it('checkbox reflects traceEnabled state when true', () => {
    useAppStore.setState({ traceEnabled: true })
    render(<OptionsPanel />)
    const checkbox = screen.getByLabelText('Show trajectory') as HTMLInputElement
    expect(checkbox.checked).toBe(true)
  })

  it('clicking checkbox calls setTraceEnabled', () => {
    render(<OptionsPanel />)
    const checkbox = screen.getByLabelText('Show trajectory')
    fireEvent.click(checkbox)
    expect(useAppStore.getState().traceEnabled).toBe(true)
  })

  it('clear button is disabled when history is empty', () => {
    useAppStore.setState({ trajectoryHistory: [] })
    render(<OptionsPanel />)
    const clearButton = screen.getByText('Clear')
    expect(clearButton).toBeDisabled()
  })

  it('clear button is enabled when history has points', () => {
    useAppStore.setState({
      trajectoryHistory: [{ x: 1, y: 1, timestamp: Date.now() }],
    })
    render(<OptionsPanel />)
    const clearButton = screen.getByText('Clear')
    expect(clearButton).not.toBeDisabled()
  })

  it('clicking clear button calls clearTrajectory', () => {
    useAppStore.setState({
      trajectoryHistory: [
        { x: 1, y: 1, timestamp: Date.now() },
        { x: 2, y: 2, timestamp: Date.now() },
      ],
    })
    render(<OptionsPanel />)
    const clearButton = screen.getByText('Clear')
    fireEvent.click(clearButton)
    expect(useAppStore.getState().trajectoryHistory).toHaveLength(0)
  })

  it('displays point count when trace is enabled and has points', () => {
    useAppStore.setState({
      traceEnabled: true,
      trajectoryHistory: [
        { x: 1, y: 1, timestamp: Date.now() },
        { x: 2, y: 2, timestamp: Date.now() },
        { x: 3, y: 3, timestamp: Date.now() },
      ],
    })
    render(<OptionsPanel />)
    expect(screen.getByText('3 points')).toBeInTheDocument()
  })

  it('displays singular point for single point', () => {
    useAppStore.setState({
      traceEnabled: true,
      trajectoryHistory: [{ x: 1, y: 1, timestamp: Date.now() }],
    })
    render(<OptionsPanel />)
    expect(screen.getByText('1 point')).toBeInTheDocument()
  })

  it('does not display point count when trace is disabled', () => {
    useAppStore.setState({
      traceEnabled: false,
      trajectoryHistory: [
        { x: 1, y: 1, timestamp: Date.now() },
        { x: 2, y: 2, timestamp: Date.now() },
      ],
    })
    render(<OptionsPanel />)
    expect(screen.queryByText('2 points')).not.toBeInTheDocument()
  })

  it('returns null when isCollapsed is true', () => {
    const { container } = render(<OptionsPanel isCollapsed={true} />)
    expect(container.firstChild).toBeNull()
  })
})
