# Multi-PCB Transform Support for KiCad 3D Viewer

## Overview

This feature allows designing multiple separate PCBs in a single `.kicad_pcb` board file and
visualizing their assembled position and orientation in the 3D viewer. Each sub-PCB is defined
by a named zone on the `User.Comments` layer, with a text field inside specifying the 3D
transformation to apply.

## How to Use

### 1. Design your PCBs

Design all your PCBs (e.g., main board, daughter boards, flex connectors) in a single board file,
laid out flat side by side.

### 2. Define Transform Areas

For each sub-PCB that needs to be repositioned in the 3D view:

1. **Create a Rule Area / Zone** on the `User.Comments` layer
2. **Name the zone** starting with `transform` (e.g., `transform`, `transform_daughterboard`,
   `transform_display_pcb`)
3. Draw the zone outline around all components belonging to that sub-PCB

### 3. Add Transform Text

Inside each transform zone, place a **Text item** on the `User.Comments` layer with the
transformation specification:

```
transform: x 5mm y -5mm z 10mm a 90° b 0° c 0°
```

#### Transform Parameters

| Parameter | Description              | Unit    | Example  |
|-----------|--------------------------|---------|----------|
| `x`       | Translation along X axis | mm      | `x 5mm`  |
| `y`       | Translation along Y axis | mm      | `y -5mm` |
| `z`       | Translation along Z axis | mm      | `z 10mm` |
| `a`       | Rotation around X axis (roll)  | degrees | `a 90°`  |
| `b`       | Rotation around Y axis (pitch) | degrees | `b 0°`   |
| `c`       | Rotation around Z axis (yaw)   | degrees | `c 0°`   |

- All parameters are optional; omitted values default to 0
- Units: `mm` for distances, `°` or `deg` for angles (or omit for default)
- Inches are also supported: `x 0.5in`
- Rotation order: Z (yaw/c) first, then Y (pitch/b), then X (roll/a)

### Origin of Transformation

The **anchor point of the transform text** is used as the origin (pivot point) for rotations.
This means rotations are applied around the text item's position, not the center of the zone.

Place the transform text at the point you want the sub-PCB to rotate around — for example,
at the edge where a daughter board connects to the main board, or at the center of a hinge.

Translation (`x`, `y`, `z`) is applied after rotation and is independent of the rotation origin.

### 4. Enable in 3D Viewer

1. Open the 3D Viewer (Alt+3 or View → 3D Viewer)
2. In the **Appearance** panel on the right, use the **"Multi-PCB transforms"** slider to move the sub-PCBs into their assembled positions

## Transform Coordinate System

The transform is relative to the item's current position on the flat board:
- **X**: Positive = right in the 3D view
- **Y**: Positive = up in the 3D view (note: inverted from PCB editor Y)
- **Z**: Positive = towards the viewer (up from the board surface)
- **a (roll)**: Tilting left/right (rotation around X axis)
- **b (pitch)**: Tilting forward/backward (rotation around Y axis)
- **c (yaw)**: Rotation in the XY plane (rotation around Z axis)

## Example

A daughter board that sits 10mm above the main board, offset 20mm to the right, and rotated 90°:

```
transform: x 20mm y 0mm z 10mm a 90° b 0° c 0°
```

A display module mounted vertically (standing up from the board):

```
transform: x 0mm y 0mm z 5mm a 0° b 90° c 0°
```

## Transform Slider

The Appearance panel in the 3D viewer has a **"Multi-PCB transform"** slider (0-100%).
At 0% the board is shown in its flat layout. At 100% all sub-PCBs are in their fully
assembled positions. Intermediate values give a smooth interpolation (exploded view).

## Current Limitations (V2)

- **Mouse picking**: Clicking on a transformed element selects the item at its original
  (flat layout) position, not the visual position.
- **OpenGL renderer only**: The transform is applied in the OpenGL renderer. The raytracing
  renderer does not yet support multi-PCB transforms.
- **Through-holes at boundaries**: Holes straddling the boundary between two transform areas
  are assigned to whichever area contains the hole's center point.

## Files Modified

| File | Change |
|------|--------|
| `3d-viewer/3d_canvas/multi_pcb_transform.h` | Transform data structures, parser, matrix builder (with interpolation) |
| `3d-viewer/3d_canvas/multi_pcb_transform.cpp` | Implementation of transform parsing, scanning, interpolated matrix building |
| `3d-viewer/3d_canvas/board_adapter.h` | Multi-PCB area storage, accessors, transform factor |
| `3d-viewer/3d_canvas/board_adapter.cpp` | Multi-PCB scanning in InitSettings() with interpolation factor |
| `3d-viewer/3d_viewer/eda_3d_viewer_settings.h` | `multi_pcb_transform_factor` float setting (0.0-1.0) |
| `3d-viewer/3d_viewer/eda_3d_viewer_settings.cpp` | Persistence for the float setting |
| `3d-viewer/dialogs/appearance_controls_3D.h` | Slider + label member variables |
| `3d-viewer/dialogs/appearance_controls_3D.cpp` | Slider UI for transform interpolation |
| `3d-viewer/3d_rendering/opengl/render_3d_opengl.h` | Per-area display list storage |
| `3d-viewer/3d_rendering/opengl/render_3d_opengl.cpp` | Per-area rendering for layers, board body, solder mask |
| `3d-viewer/3d_rendering/opengl/create_scene.cpp` | Per-area geometry splitting during scene creation |

## Architecture

```
Board File (.kicad_pcb)
    │
    ├── Zone (User.Comments, name="transform_xxx")
    │     └── Text (User.Comments, "transform: x ... y ... z ... a ... b ... c ...")
    │
    ▼
ScanMultiPcbAreas()  →  vector<MULTI_PCB_AREA>
    │                        ├── outline (SHAPE_POLY_SET)
    │                        ├── transform (x,y,z,a,b,c)
    │                        ├── center
    │                        └── rotationCenter (text anchor point)
    ▼
BuildTransformMatrix(factor)  →  glm::mat4 per area (interpolated by slider)
    │
    ▼
BOARD_ADAPTER stores areas + matrices
    │
    ├──▶ RENDER_3D_OPENGL::get3dModelsFromFootprint()
    │       └── Applies transform to footprint 3D models
    │
    ├──▶ RENDER_3D_OPENGL::generatePerAreaLayerLists()
    │       └── Splits copper/silk/tech layer geometry per area
    │
    ├──▶ Board body split per area (BooleanIntersection)
    │       └── Each sub-board gets its own display list
    │
    └──▶ RENDER_3D_OPENGL::Redraw()
            └── Renders each area's geometry with glMultMatrixf(transform)
```
