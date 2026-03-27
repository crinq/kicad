# Multi-PCB Feature - Proposals and Future Improvements

## Current State (V1)

The initial implementation applies 3D transforms to footprint models only. This document
collects ideas for improving and extending the multi-PCB workflow.

---

## High Priority Improvements

### 1. Transform Board Geometry (Copper, Silkscreen, Board Body)

**Problem**: Currently only 3D models move; the PCB artwork (copper, silkscreen, solder mask,
board outline) stays in the flat layout position.

**Proposed Approach**: During `BOARD_ADAPTER::createLayers()`, split the 2D layer containers
(`BVH_CONTAINER_2D`) by multi-PCB area. Create separate `OPENGL_RENDER_LIST` display lists
per area. During rendering, apply the area's transform matrix before drawing each area's
display list.

**Complexity**: High - requires changes to the core layer creation pipeline.

**Alternative**: Use OpenGL clip planes or stencil buffer to mask each area during rendering,
rendering the full layer once per area with different transforms. Simpler but less efficient.

### 2. Split Board Outline Per Area

**Problem**: The board body is rendered as one piece based on the `Edge.Cuts` layer.

**Proposed Approach**: Detect which parts of the board outline belong to which transform area
and create separate board body meshes. Each mesh gets its own transform.

**Benefit**: Each sub-PCB appears as a physically separate board in the 3D view.

### 3. Proper Ray Casting for Transformed Models

**Problem**: Mouse picking works on untransformed positions. Clicking on a visually transformed
model doesn't select it.

**Proposed Approach**: For each transform area, compute the inverse transform matrix. When
casting a ray for mouse picking, test the ray against each area's inverse-transformed geometry.
Return the closest hit across all areas.

---

## Medium Priority Improvements

### 4. Connector Mating Visualization

**Problem**: Users often want to see how connectors on different PCBs mate together.

**Proposed Approach**: Allow a "snap" mode where one PCB's connector aligns with another PCB's
connector. The user specifies matching connector pairs, and the transform is auto-calculated.

**UX**: Right-click a connector pad → "Snap to connector on other PCB" → Select target pad.

### 5. Interactive Transform Manipulation

**Problem**: Editing the transform text is cumbersome. Users need to manually calculate angles
and offsets.

**Proposed Approach**: Add a 3D gizmo (translate/rotate handles) in the 3D viewer that allows
dragging sub-PCBs to their assembled positions. The resulting transform is written back to the
text field on the board.

**UX**: Select a transform area → Drag handles appear → Move/rotate → Auto-updates text.

### 6. Enclosure / Mechanical Parts

**Problem**: Users want to visualize PCBs inside enclosures or with mechanical mounting parts.

**Proposed Approach**: Allow STEP/3D model references in the transform text field:

```
transform: x 0mm y 0mm z 0mm a 0° b 0° c 0° model: enclosure.step
```

Or as a separate text item in the zone:

```
model: /path/to/enclosure.step offset: x 0mm y 0mm z -5mm
```

### 7. Flex PCB Bend Visualization

**Problem**: Flex PCBs need to be bent along fold lines, not just rigidly transformed.

**Proposed Approach**: Define bend lines on the flex area. The 3D viewer deforms the geometry
along these lines. This is significantly more complex than rigid transforms.

**Simpler Alternative**: Approximate bends as a series of small rigid segments (piecewise linear
approximation of the bend curve).

### 8. Animation / Assembly Sequence

**Problem**: Users want to visualize the assembly order or create assembly instructions.

**Proposed Approach**: Allow defining keyframes for transforms. The 3D viewer can interpolate
between them to create an assembly animation:

```
transform_step1: x 0mm y 0mm z 50mm a 0° b 0° c 0°
transform_step2: x 0mm y 0mm z 10mm a 0° b 0° c 0°
transform_final: x 0mm y 0mm z 0mm a 0° b 0° c 0°
```

---

## Low Priority / Exploratory Ideas

### 9. Multi-Board Electrical Connectivity

**Problem**: Signals crossing between PCBs (via connectors) aren't tracked.

**Proposed Approach**: Define inter-board net mappings. The schematic could reference multiple
board files, and DRC could verify that connector pinouts match.

### 10. Thermal Simulation Hints

Allow specifying thermal coupling between stacked PCBs for thermal analysis export.

### 11. Weight / Center of Gravity Display

With 3D model data and component weights from the BOM, calculate and display the center of
gravity of the assembled multi-PCB system.

### 12. Export Assembled Position

Export the assembled (transformed) positions for:
- Pick-and-place files relative to the assembled coordinate system
- STEP export with all PCBs in their assembled positions
- Assembly documentation / drawings

### 13. Import Transform from STEP Assembly

If the user has already created a mechanical assembly in a CAD tool, import the PCB positions
from the STEP file to auto-populate the transform text fields.

---

## UX Alternatives to Consider

### Zone-Based vs. Sheet-Based

**Current (zone-based)**: Transform areas are zones drawn on User.Comments. Simple, uses
existing KiCad primitives.

**Alternative (sheet-based)**: Each sub-PCB is a "sheet" or "board block" with first-class
support in the data model. More integrated but requires schema changes.

### Text-Based vs. Properties-Based Transform

**Current (text-based)**: Transform is a text string inside the zone. Easy to implement, but
fragile to parse and not discoverable.

**Alternative (properties-based)**: Add transform properties to the zone properties dialog.
Uses structured fields instead of text parsing. More robust and user-friendly.

### Transform Origin

**Current**: Transform is applied relative to the item's current position.

**Alternative**: Allow specifying a transform origin point (e.g., center of the zone, or a
specific pad). This makes it easier to specify rotations around mounting holes or connector
pins.

```
transform: origin pad:J1:1 x 0mm y 0mm z 10mm a 90° b 0° c 0°
```

---

## Raytracing Renderer Support

The raytracing renderer creates 3D objects from 2D layer containers differently from OpenGL.
To support transforms in raytracing:

1. During `RENDER_3D_RAYTRACE_BASE::Reload()`, detect multi-PCB areas
2. For each 3D object created, check if its board item is in a transform area
3. Apply the transform to the object's position and orientation before inserting into the BVH
4. Update `IntersectBoardItem()` to account for transforms

This is more involved because the raytrace renderer uses pre-built acceleration structures.
