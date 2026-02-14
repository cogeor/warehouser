# TASK: Frontend Redesign - Professional CAD/Simulation Aesthetic

Created: 2026-02-14
Status: Planning

## Summary

Redesign the warehouse simulation frontend with a clean, professional aesthetic inspired by CAD and simulation software. The current design has issues with:
- Visual clutter and tasteless button colors
- Dark theme on the board area
- World draggable by mouse (should be fixed/centered)
- Too many unnecessary controls

## Requirements

### Visual Design
- **White theme** throughout, including the simulation board
- **Professional CAD aesthetic** - think Fusion 360, SolidWorks, ROS2 Rviz
- Clean, minimal interface with plenty of whitespace
- Subtle, professional color palette (no garish colors)
- Fixed world position, centered on screen

### Interaction
- World should NOT be draggable by mouse
- World should be centered in the viewport
- Only keep absolutely necessary controls
- Professional button styling (subtle, not colorful)

### Controls to Evaluate
Review current controls and determine which are essential:
- Play/Pause simulation
- Reset simulation
- Speed controls
- Camera/zoom controls
- Debug overlays (lidar, etc.)

## Deliverable

Present 5 design options for user selection before implementation:

1. **Minimal Studio** - Absolute minimal controls, floating panel
2. **CAD Classic** - Traditional toolbar layout like CAD software
3. **Split View** - Simulation + sidebar with controls
4. **Floating Panels** - Detached, movable panels
5. **Immersive** - Full-screen simulation, controls on hover

Each design should include:
- Layout sketch/description
- Color palette
- Control placement
- Typography choices

## Current State Analysis Needed

Before designing, analyze:
- Current component structure
- Current styling approach (Tailwind CSS)
- Current controls and their purposes
- Current color scheme

## Success Criteria

- [ ] User approves one of the 5 designs
- [ ] White theme implemented
- [ ] World centered and non-draggable
- [ ] Unnecessary controls removed
- [ ] Professional CAD-like aesthetic achieved
- [ ] All tests still pass
