# DownPour Development Tools

Three standalone tools for development, debugging, and content creation for the DownPour driving simulator.

## 📊 SystemMonitor - Hardware Performance Metrics

**Language:** C (with Objective-C for macOS APIs)
**Location:** `SystemMonitor/`

Real-time hardware detection with color-coded performance metrics.

### What It Does
- Detects Vulkan backend type (GPU vs CPU)
- Shows real CPU/GPU usage percentages
- Displays power consumption estimates
- Color-codes metrics: 🟢 Green (<40%), 🟡 Yellow (40-80%), 🔴 Red (>80%)

### Usage
```bash
# Via Makefile
make run-monitor

# Direct
./src/tools/SystemMonitor/monitor
```

### Output Example
```
==============================================
  SYSTEM MONITOR - Hardware Metrics Report
==============================================

Vulkan Backend: GPU (Hardware Accelerated)

CPU Usage:      14.1% [Green]
CPU Frequency:  3000 MHz
CPU Power:      6.9 W

GPU Usage:      45.2% [Yellow]
GPU Frequency:  1400 MHz
GPU Power:      8.7 W
==============================================
```

---

## 🎨 SceneEditor - Interactive Transform Editor

**Language:** Rust (with egui GUI framework)
**Location:** `SceneEditor/`

Interactive GUI for editing 3D scene object transforms.

### What It Does
- Load scene configurations from JSON files
- Edit object transforms (position, rotation, scale)
- Visual object list with selection
- Save modified configurations back to JSON
- Real-time editing with drag values

### Usage
```bash
# Via Makefile
make run-editor

# Direct with config file
./src/tools/SceneEditor/target/release/scene_editor assets/models/bmw/bmw.glb.json

# Direct with example config
./src/tools/SceneEditor/target/release/scene_editor
```

### Interface
- **Left Panel:** Object list, model config, camera config
- **Center Panel:** Transform editor
  - Position: X, Y, Z (drag to adjust)
  - Rotation: Yaw, Pitch, Roll in degrees
  - Scale: X, Y, Z multipliers
- **Top Menu:** Save, Reload, New Object buttons

---

## 🔄 GLTFTools - Model to JSON Converter

**Language:** Python
**Location:** `GLTFTools/`

Convert GLTF/GLB 3D model files to comprehensive JSON format.

### What It Does
- Reads binary `.glb` or text `.gltf` files
- Extracts all model data (nodes, meshes, materials, textures)
- Outputs readable JSON with full scene hierarchy
- Handles large files efficiently (metadata only, no raw vertices)

### Usage
```bash
# Via Makefile
make run-converter  # Shows usage

# Convert BMW model
python3 src/tools/GLTFTools/python/converter.py assets/models/bmw/bmw.glb output.json

# Auto-generate output filename
python3 src/tools/GLTFTools/python/converter.py model.glb
# Creates: model.json
```

### Output Stats (BMW Model)
- **Nodes:** 513
- **Meshes:** 219
- **Materials:** 18
- **Output Size:** 526KB JSON

---

## 🛠️ Makefile Targets

### Build All Tools
```bash
make tools              # Build all three tools
make tools-monitor      # Build SystemMonitor (C)
make tools-editor       # Build SceneEditor (Rust)
make tools-converter    # Prepare GLTFTools (Python)
```

### Run Tools
```bash
make run-monitor        # Run SystemMonitor
make run-editor         # Launch SceneEditor GUI
make run-converter      # Show GLTFConverter usage
```

### Maintenance
```bash
make tools-clean        # Clean all tool builds
make install-tools      # Install Python dependencies
make build-all          # Build DownPour + all tools
```

---

## 📋 Dependencies

### SystemMonitor
- Vulkan SDK
- macOS IOKit, Metal frameworks (for hardware detection)
- Xcode Command Line Tools

### SceneEditor
- Rust toolchain (install: `curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh`)
- egui 0.30 (auto-installed via Cargo)

### GLTFTools
- Python 3.12+
- pygltflib (`pip install pygltflib` or `uv pip install pygltflib`)

---

## 🔗 Tool Integration

These tools work together in a typical workflow:

```
1. GLTFTools
   ↓ Convert model.glb → scene.json

2. SceneEditor
   ↓ Edit transforms → save scene.json

3. DownPour
   ↓ Load scene.json → render

4. SystemMonitor
   ↓ Monitor GPU/CPU during rendering
```

---

## 📁 Directory Structure

```
src/tools/
├── README.md              # This file
├── SystemMonitor/         # C hardware monitoring
│   ├── monitor.c
│   ├── monitor.h
│   ├── macos_metrics.m
│   ├── vulkan_detector.c
│   ├── main.c
│   ├── Makefile
│   └── README.md
├── SceneEditor/           # Rust scene transform editor
│   ├── src/
│   │   ├── main.rs
│   │   ├── ui.rs
│   │   └── scene_data.rs
│   ├── Cargo.toml
│   ├── Makefile
│   └── README.md
└── GLTFTools/             # Python GLTF converter
    ├── python/
    │   ├── converter.py
    │   └── __init__.py
    ├── pyproject.toml
    └── README.md
```

---

## 📝 Individual READMEs

Each tool has its own detailed README:
- `SystemMonitor/README.md` - C compilation, macOS APIs, color output
- `SceneEditor/README.md` - Rust build, GUI usage, JSON format
- `GLTFTools/README.md` - Python usage, output format, dependencies

---

**Last Updated:** January 25, 2026
