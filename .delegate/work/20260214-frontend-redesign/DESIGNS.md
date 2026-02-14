# Frontend Redesign Proposals

## Current State Issues
- Dark theme (needs white)
- World is draggable (should be fixed, centered)
- Colorful buttons (green/yellow/red/purple - tasteless)
- Too many controls (layer toggles, zoom, demo mode)
- Cluttered sidebar with 3 panels

---

## Design 1: Minimal Studio

**Inspiration**: Figma, Linear, modern design tools

```
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│                                                             │
│          ┌─────────────────────────────────┐               │
│          │                                 │               │
│          │                                 │               │
│          │        SIMULATION CANVAS        │               │
│          │         (centered, fixed)       │               │
│          │         white background        │               │
│          │                                 │               │
│          │                                 │               │
│          └─────────────────────────────────┘               │
│                                                             │
│     ┌──────────────────────────┐                           │
│     │  ▶  ⏸  ↺   │  0:00:00  │                           │
│     └──────────────────────────┘                           │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

**Color Palette**:
- Background: `#FAFAFA` (off-white)
- Canvas: `#FFFFFF` (pure white)
- Canvas border: `#E5E7EB` (gray-200)
- Text: `#1F2937` (gray-800)
- Icons: `#6B7280` (gray-500)
- Accent: `#3B82F6` (blue-500) - for selected/active only

**Controls** (bottom center floating):
- Play/Pause (single toggle button)
- Reset
- Simulation time display
- Connection indicator (subtle dot)

**Removed**:
- Zoom controls (fixed 1:1)
- Layer toggles
- Demo mode
- Objective panel
- Robot selector dropdown

**Typography**: Inter or system-ui, 14px base

---

## Design 2: CAD Classic

**Inspiration**: Fusion 360, SolidWorks, AutoCAD

```
┌─────────────────────────────────────────────────────────────┐
│ ◉ Warehouser │  ▶ Pause  Reset  │         │ ● Connected   │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  ┌─────────────────────────────────────────────────────┐   │
│  │                                                     │   │
│  │                                                     │   │
│  │              SIMULATION CANVAS                      │   │
│  │               (white, centered)                     │   │
│  │                                                     │   │
│  │                                                     │   │
│  │                                                     │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                             │
├─────────────────────────────────────────────────────────────┤
│ Time: 00:00:00  │  Selected: robot_0  │  Pos: (5.0, 3.2)  │
└─────────────────────────────────────────────────────────────┘
```

**Color Palette**:
- Background: `#F3F4F6` (gray-100)
- Canvas: `#FFFFFF` (white)
- Toolbar: `#FFFFFF` with bottom border `#E5E7EB`
- Status bar: `#F9FAFB` (gray-50)
- Text: `#374151` (gray-700)
- Icons: `#4B5563` (gray-600)
- Active button: `#E5E7EB` (gray-200) background

**Controls** (top toolbar):
- Play/Pause toggle
- Reset button
- Connection status

**Status bar** (bottom):
- Simulation time
- Selected entity info
- Position readout

**Removed**:
- Layer toggles
- Zoom controls
- Demo mode
- Objective panel

**Typography**: SF Pro / system-ui, 13px base

---

## Design 3: Split View

**Inspiration**: Rviz, Gazebo, ROS tools

```
┌──────────────────────────────────────────┬──────────────────┐
│                                          │  CONTROLS        │
│                                          │                  │
│                                          │  ▶ Run           │
│                                          │  ⏸ Pause         │
│        SIMULATION CANVAS                 │  ↺ Reset         │
│         (white, takes ~75%)              │                  │
│                                          │  ─────────────   │
│                                          │                  │
│                                          │  STATUS          │
│                                          │  Time: 00:00:00  │
│                                          │  Robot: robot_0  │
│                                          │  State: IDLE     │
│                                          │                  │
└──────────────────────────────────────────┴──────────────────┘
```

**Color Palette**:
- Background: `#FFFFFF`
- Sidebar: `#F9FAFB` (gray-50)
- Divider: `#E5E7EB` (gray-200)
- Text: `#111827` (gray-900)
- Muted text: `#6B7280` (gray-500)
- Buttons: `#F3F4F6` background, `#374151` text

**Controls** (right sidebar ~200px):
- Run/Pause/Reset as list
- Status section below

**Removed**:
- Zoom
- Layer toggles
- Demo mode
- Objective panel
- Color selector

**Typography**: JetBrains Mono for values, Inter for labels

---

## Design 4: Floating Toolbar

**Inspiration**: Blender, Unity Editor

```
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│   ┌─────────────────────────────────────────────────────┐  │
│   │                                                     │  │
│   │                                                     │  │
│   │              SIMULATION CANVAS                      │  │
│   │               (white, full area)                    │  │
│   │                                                     │  │
│   │                                                     │  │
│   │                                                     │  │
│   └─────────────────────────────────────────────────────┘  │
│                                                             │
│   ┌─────────────┐                        ┌───────────────┐ │
│   │ ▶ ⏸ ↺      │                        │ 00:00:00 ● │ │
│   └─────────────┘                        └───────────────┘ │
└─────────────────────────────────────────────────────────────┘
```

**Color Palette**:
- Background: `#FAFAFA`
- Canvas: `#FFFFFF`
- Floating panels: `#FFFFFF` with shadow
- Text: `#1F2937`
- Icons: `#6B7280`

**Controls** (floating pills):
- Bottom-left: Transport controls (Play/Pause/Reset)
- Bottom-right: Time + connection status

**Removed**:
- All layer controls
- Zoom
- Demo mode
- Objective/Status panels

**Typography**: System default, 14px

---

## Design 5: Immersive Full-Screen

**Inspiration**: Game engines, simulators

```
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│                                                             │
│                                                             │
│                  SIMULATION CANVAS                          │
│                   (white, full viewport)                    │
│                                                             │
│                                                             │
│                                                             │
│                                                             │
│                                                             │
│                     ┌───────────────┐                       │
│                     │  ▶  ⏸  ↺     │                       │
│                     └───────────────┘                       │
└─────────────────────────────────────────────────────────────┘

     ┌─────────────────┐ (appears on hover bottom)
     │ Time: 00:00:00  │
     │ Status: Running │
     └─────────────────┘
```

**Color Palette**:
- Background: `#FFFFFF` (pure white everywhere)
- Canvas grid: `#F3F4F6` (very light gray)
- Controls: `#FFFFFF` with subtle shadow
- Text: `#374151`
- Icons: `#9CA3AF` (gray-400), `#374151` on hover

**Controls**:
- Centered bottom transport (visible)
- Status tooltip on hover

**Removed**:
- Header
- All panels
- Zoom/layer controls
- Demo mode

**Typography**: SF Mono for time, system for rest

---

## Comparison Matrix

| Feature | Design 1 | Design 2 | Design 3 | Design 4 | Design 5 |
|---------|----------|----------|----------|----------|----------|
| Header | No | Yes (toolbar) | No | No | No |
| Sidebar | No | No | Yes | No | No |
| Controls location | Bottom center | Top bar | Right panel | Floating | Bottom center |
| Status info | Minimal | Status bar | Sidebar | Floating | On hover |
| Complexity | Very Low | Low | Medium | Low | Very Low |
| CAD-like feel | ⭐⭐ | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐ |
| Modern feel | ⭐⭐⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐⭐ | ⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ |

---

## Recommendation

**Design 2 (CAD Classic)** is recommended because:
1. Most professional/familiar for engineering users
2. Clear hierarchy with toolbar and status bar
3. Maximum canvas space while showing useful info
4. Matches user's "professional CAD/simulation software" request
