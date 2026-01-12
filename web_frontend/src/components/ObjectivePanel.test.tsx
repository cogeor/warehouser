import { describe, it, expect, vi, beforeEach } from 'vitest'
import { render, screen, fireEvent } from '@testing-library/react'
import { ObjectivePanel } from './ObjectivePanel'

// Mock the ROS connection
vi.mock('../ros/connection', () => ({
  publishCommand: vi.fn(),
}))

import { publishCommand } from '../ros/connection'

describe('ObjectivePanel', () => {
  beforeEach(() => {
    vi.clearAllMocks()
  })

  it('renders color selector with default red', () => {
    render(<ObjectivePanel />)
    const select = screen.getByRole('combobox')
    expect(select).toHaveValue('red')
  })

  it('renders pick button', () => {
    render(<ObjectivePanel />)
    expect(screen.getByText('Pick')).toBeInTheDocument()
  })

  it('has red, green, blue options', () => {
    render(<ObjectivePanel />)
    expect(screen.getByText('Red')).toBeInTheDocument()
    expect(screen.getByText('Green')).toBeInTheDocument()
    expect(screen.getByText('Blue')).toBeInTheDocument()
  })

  it('publishes command with selected color on pick click', () => {
    render(<ObjectivePanel />)
    fireEvent.click(screen.getByText('Pick'))
    expect(publishCommand).toHaveBeenCalledWith('red')
  })

  it('publishes command with changed color', () => {
    render(<ObjectivePanel />)
    const select = screen.getByRole('combobox')
    fireEvent.change(select, { target: { value: 'green' } })
    fireEvent.click(screen.getByText('Pick'))
    expect(publishCommand).toHaveBeenCalledWith('green')
  })
})
