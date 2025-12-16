# GLTFTools - GLTF/GLB Model Converter

Convert GLTF/GLB 3D model files to comprehensive JSON format.

## What It Does

Reads binary `.glb` or text `.gltf` files and extracts all model data into a readable JSON format including:
- **Nodes** - Scene hierarchy with transforms (position, rotation, scale)
- **Meshes** - Geometry primitives with vertex attributes
- **Materials** - PBR materials with texture references
- **Textures & Images** - Texture data and image URIs
- **Buffers & Accessors** - Binary data metadata
- **Extensions** - GLTF extensions used

## Usage

### Basic Conversion
```bash
python3 python/converter.py <input.glb> [output.json]
```

### Examples
```bash
# Convert BMW model (from tools directory)
python3 python/converter.py ../../../assets/models/bmw/bmw.glb bmw_data.json

# Auto-generate output filename
python3 python/converter.py model.glb
# Creates: model.json

# From project root
python3 src/tools/GLTFTools/python/converter.py assets/models/bmw/bmw.glb output.json
```

## Output Format

```json
{
  "asset": {
    "version": "2.0",
    "generator": "Khronos glTF Blender I/O..."
  },
  "scenes": [
    { "index": 0, "nodes": [0, 1, 2, ...] }
  ],
  "nodes": [
    {
      "index": 0,
      "name": "BMW_Body",
      "translation": [0, 0, 0],
      "rotation": [0, 0, 0, 1],
      "scale": [1, 1, 1],
      "mesh": 0,
      "children": [1, 2]
    }
  ],
  "meshes": [...],
  "materials": [...],
  "textures": [...],
  "images": [...],
  "buffers": [...],
  "accessors": [...]
}
```

## Dependencies

Install with pip:
```bash
pip install pygltflib
```

Or using uv (recommended):
```bash
uv pip install pygltflib
```

## Performance

- **BMW Model**: 513 nodes, 219 meshes, 18 materials → 526KB JSON
- **Processing Time**: ~1 second for large models
- **Memory Efficient**: Metadata only, no raw binary vertex data

## Files

```
GLTFTools/
├── python/
│   ├── converter.py    # Main conversion script (15KB)
│   └── __init__.py     # Package initialization
├── pyproject.toml      # Python dependencies
└── README.md           # This file
```