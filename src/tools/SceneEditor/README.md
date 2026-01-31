# SceneEditor - Interactive Transform Editor

Rust-based GUI application for editing 3D scene object transforms.

## What It Does

Interactive visual editor for scene configuration files with real-time 3D rendering:
- **3D Visualization**: Real-time rendering of GLTF/GLB models with wgpu
- Load JSON scene configs (like `bmw.glb.json`)
- Edit object transforms: position, rotation, scale with live preview
- Modify model and camera settings
- Interactive camera controls (orbital + scene camera modes)
- Save changes back to JSON
- Track unsaved changes
- Add/delete objects

## Usage

### Launch with Config File
```bash
# From SceneEditor directory
cd src/tools/SceneEditor

# Via Makefile (recommended)
make run-bmw

# Or build and run manually
cargo build --release
./target/release/scene_editor ../../../assets/models/bmw/bmw.glb.json

# Run with example config
./target/release/scene_editor
```

### First-Time Setup
```bash
# Install Rust if not already installed
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
source $HOME/.cargo/env

# Build release binary
cargo build --release
```

## Interface Guide

### Top Menu Bar
- **💾 Save** - Write changes to JSON file
- **🔄 Reload** - Discard changes and reload from file
- **📋 New Object** - Add new object to scene
- **📦 Load Model** - Open file browser to select GLTF/GLB model
- **Status** - Shows save status and unsaved changes indicator

### Left Panel - Object List
**Model Configuration:**
- Target Length
- Orientation (Euler angles X/Y/Z)

**Camera Configuration:**
- Position (X/Y/Z)
- FOV (Field of View, 30-120°)

**Objects List:**
- Click to select object
- Selected object highlights

### Right Panel - 3D Viewport
Real-time 3D visualization:
- **Camera Mode Toggle**: Switch between Scene Camera and Orbital modes
- **Interactive Controls**:
  - Left mouse drag: Orbit/rotate camera
  - Right mouse drag: Pan view
  - Mouse scroll: Zoom in/out
  - Reset Camera button
- **Model Display**: Rendered GLTF model with lighting
- **Real-time Updates**: See transform changes immediately

### Center Panel - Transform Editor
When an object is selected:

**Position:**
- X, Y, Z coordinates
- Drag to adjust (step: 0.01)

**Rotation (Euler Angles):**
- Yaw (rotation around Y-axis)
- Pitch (rotation around X-axis)
- Roll (rotation around Z-axis)
- Values in degrees
- Drag to adjust (step: 1.0°)

**Scale:**
- X, Y, Z multipliers
- Drag to adjust (step: 0.01)

**Controls:**
- Enabled checkbox - Toggle object visibility
- 🗑 Delete Object button

## JSON Format

### Input Format
```json
{
  "model": {
    "targetLength": 4.7,
    "orientation": {
      "euler": [0, 180, 0],
      "unit": "degrees"
    }
  },
  "camera": {
    "cockpit": {
      "position": { "xyz": [1.56, 4.78, -2.28] },
      "fov": 75.0
    }
  },
  "objects": [
    {
      "name": "BMW_Model",
      "transform": {
        "position": [0, 0, 0],
        "rotationEuler": [0, 180, 0],
        "scale": [1, 1, 1]
      },
      "enabled": true
    }
  ]
}
```

## Building

```bash
# Debug build
cargo build

# Release build (optimized)
cargo build --release

# Run without building binary
cargo run

# Clean build artifacts
cargo clean
```

### Build Requirements
- Rust 1.70+ (2021 edition)
- Cargo (comes with Rust)

### Dependencies (auto-installed)
- **eframe** 0.30 - Application framework (with wgpu backend)
- **egui** 0.30 - Immediate mode GUI
- **wgpu** 23 - Modern graphics API (Metal on macOS)
- **gltf** 1.4 - GLTF model loading
- **bytemuck** 1.14 - Safe byte casting for GPU data
- **glam** 0.29 - Math library (vectors, matrices)
- **serde** 1.0 - Serialization
- **serde_json** 1.0 - JSON parsing
- **rfd** 0.15 - Native file dialogs
- **pollster** 0.3 - Async runtime

## Technical Details

### Architecture
```
main.rs
  ↓ Creates EditorApp
  └─ EditorState (ui.rs)
       ↓ Manages
       └─ SceneConfig (scene_data.rs)
            └─ SceneObject with Transform
```

### Files
- `src/main.rs` - Entry point, eframe setup
- `src/ui.rs` - egui layout and viewport integration
- `src/scene_data.rs` - Data structures and JSON I/O
- `src/renderer.rs` - **New**: wgpu-based 3D renderer with GLTF loading
- `src/shader.wgsl` - **New**: WGSL vertex/fragment shaders
- `Cargo.toml` - Dependencies and build config
- `Makefile` - Convenience build targets

### Data Flow
```
Load JSON → Deserialize → SceneConfig → EditorState
                                            ↓
                                        egui UI
                                            ↓
User edits transforms → Mark unsaved → Click Save
                                            ↓
                            Serialize → Write JSON
```

## Controls

### UI Controls
- **Drag values** - Click and drag left/right to adjust
- **Precision editing** - Type exact values in input boxes
- **Esc** - Deselect focused widget

### 3D Viewport Controls
- **Left mouse drag** - Orbit camera around model
- **Right mouse drag** - Pan camera view
- **Mouse scroll** - Zoom in/out
- **Reset Camera button** - Return to default view

## Performance

- **Startup:** Instant
- **UI Refresh:** 60 FPS
- **File I/O:** <100ms for typical configs
- **Binary Size:** 4.1 MB (release build)

## Troubleshooting

### "Cannot find scene_editor binary"
```bash
cargo build --release
# Binary at: target/release/scene_editor
```

### "Failed to load config file"
Check that:
- File path is correct
- JSON is valid (`jq . < file.json`)
- File has read permissions

### UI doesn't update
- Click Save to persist changes
- Check for unsaved changes indicator (⚠)

## Future Enhancements

- ✅ ~~3D preview window~~ (Completed!)
- Texture loading and display
- Material properties (PBR)
- Multiple light sources
- Grid and axis helpers
- Object picking in 3D view
- Transform gizmos
- Undo/redo system
- Copy/paste transforms
- Snap to grid
- Quaternion rotation mode
- Multiple config file tabs
