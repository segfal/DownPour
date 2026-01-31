# DownPour Tools - Quick Reference Index

## 📂 Directory Overview

```
src/tools/
├── README.md              ⭐ START HERE - Overview of all tools
├── TOOLS_INDEX.md         📑 This file - Quick reference
├── SystemMonitor/         🖥️  Hardware monitoring (C)
│   └── README.md
├── SceneEditor/           🎨 Transform editor (Rust/GUI)
│   └── README.md
└── GLTFTools/             🔄 Model converter (Python)
    └── README.md
```

---

## 🚀 Quick Start Commands

### Build Everything
```bash
make tools              # Build all three tools
```

### Run Individual Tools
```bash
make run-monitor        # SystemMonitor - Show CPU/GPU metrics
make run-editor         # SceneEditor - Launch GUI editor
make run-converter      # GLTFTools - Show usage help
```

---

## 📖 Documentation Map

| Tool | Purpose | README Location | Language |
|------|---------|-----------------|----------|
| **SystemMonitor** | Real-time hardware metrics with color output | `SystemMonitor/README.md` | C + Obj-C |
| **SceneEditor** | Interactive GUI for editing transforms | `SceneEditor/README.md` | Rust |
| **GLTFTools** | Convert GLTF/GLB to JSON | `GLTFTools/README.md` | Python |

---

## 🔍 What Each README Contains

### `README.md` (Main Tools Overview)
- Overview of all three tools
- Makefile targets reference
- Integration workflow
- Directory structure
- Dependencies list

### `SystemMonitor/README.md`
- Color-coded output explanation
- macOS API details (IOKit, Metal, mach)
- Vulkan backend detection
- Build requirements
- Technical limitations

### `SceneEditor/README.md`
- GUI interface guide
- JSON format specification
- Transform editing workflow
- Rust build instructions
- Keyboard shortcuts

### `GLTFTools/README.md`
- Conversion examples
- Output JSON format
- Performance stats
- Python dependencies
- Usage from different directories

---

## 💡 Common Tasks

### Convert Model to JSON
```bash
python3 src/tools/GLTFTools/python/converter.py assets/models/bmw/bmw.glb scene.json
```

### Edit Scene Transforms
```bash
./src/tools/SceneEditor/target/release/scene_editor scene.json
```

### Monitor Performance While Testing
```bash
./src/tools/SystemMonitor/monitor
```

---

## 🔗 External Links

- Main project README: `../../README.md`
- Architecture docs: `../../ARCHITECTURE.md`
- Build system: `../../Makefile`

---

**For Obsidian:** Each README is self-contained and can be viewed independently. Use [[wikilinks]] to navigate between them.
