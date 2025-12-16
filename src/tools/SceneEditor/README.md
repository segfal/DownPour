# SceneEditor - Interactive Transform Editor

Rust-based GUI application for editing 3D scene object transforms.

## What It Does

Interactive visual editor for scene configuration files:
- Load JSON scene configs (like `bmw.glb.json`)
- Edit object transforms: position, rotation, scale
- Modify model and camera settings
- Save changes back to JSON
- Track unsaved changes
- Add/delete objects

## Usage

### Launch with Config File
```bash
# From project root via Makefile
make run-editor

# Direct with specific config
./target/release/scene_editor ../../assets/models/bmw/bmw.glb.json

# Direct with example config
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
- **eframe** 0.30 - Application framework
- **egui** 0.30 - Immediate mode GUI
- **serde** 1.0 - Serialization
- **serde_json** 1.0 - JSON parsing
- **glam** 0.29 - Math library

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
- `src/main.rs` - Entry point, eframe setup (51 lines)
- `src/ui.rs` - egui layout and rendering (227 lines)
- `src/scene_data.rs` - Data structures and JSON I/O (244 lines)
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

## Keyboard Shortcuts

- **Drag values** - Click and drag left/right to adjust
- **Precision editing** - Type exact values in input boxes
- **Esc** - Deselect focused widget

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

- 3D preview window
- Undo/redo system
- Copy/paste transforms
- Snap to grid
- Quaternion rotation mode
- Multiple config file tabs
