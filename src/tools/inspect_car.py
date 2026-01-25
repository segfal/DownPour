#!/usr/bin/env python3
"""
Inspect BMW car model materials and structure
"""
import json
import sys
from pathlib import Path

def inspect_car_model():
    """Analyze the BMW car model structure"""

    # Load the car model JSON (if it exists)
    car_glb_json = Path("assets/models/bmw/bmw.glb.json")
    car_gltf_json = Path("assets/models/bmw/bmw.gltf.json")

    json_file = None
    if car_glb_json.exists():
        json_file = car_glb_json
    elif car_gltf_json.exists():
        json_file = car_gltf_json

    if not json_file:
        print("ERROR: No JSON file found for BMW model")
        print("Checking gltf file directly...")
        gltf_file = Path("assets/models/bmw/bmw.gltf")
        if gltf_file.exists():
            with open(gltf_file) as f:
                data = json.load(f)
        else:
            print("No gltf file found either")
            return
    else:
        with open(json_file) as f:
            data = json.load(f)

    print("=" * 80)
    print("BMW CAR MODEL ANALYSIS")
    print("=" * 80)

    # Materials
    if "materials" in data:
        print(f"\n{len(data['materials'])} MATERIALS FOUND:")
        print("-" * 80)
        for i, mat in enumerate(data['materials']):
            name = mat.get('name', f'Material_{i}')

            # Check for transparency indicators
            alpha_mode = mat.get('alphaMode', 'OPAQUE')
            alpha_cutoff = mat.get('alphaCutoff', 0.5)

            pbr = mat.get('pbrMetallicRoughness', {})
            base_color_factor = pbr.get('baseColorFactor', [1, 1, 1, 1])
            alpha_value = base_color_factor[3] if len(base_color_factor) > 3 else 1.0

            double_sided = mat.get('doubleSided', False)

            # Detect glass materials
            is_glass = any(keyword in name.lower() for keyword in ['glass', 'window', 'windshield', 'transparent'])

            print(f"\n[{i}] {name}")
            print(f"    Alpha Mode: {alpha_mode}")
            print(f"    Alpha Value: {alpha_value}")
            print(f"    Alpha Cutoff: {alpha_cutoff}")
            print(f"    Double Sided: {double_sided}")
            print(f"    Likely Glass: {is_glass}")

            if pbr:
                if 'baseColorTexture' in pbr:
                    print(f"    Base Color Texture: {pbr['baseColorTexture']}")
                if 'metallicFactor' in pbr:
                    print(f"    Metallic: {pbr['metallicFactor']}")
                if 'roughnessFactor' in pbr:
                    print(f"    Roughness: {pbr['roughnessFactor']}")

    # Nodes (for structure)
    if "nodes" in data:
        print(f"\n\n{len(data['nodes'])} NODES FOUND:")
        print("-" * 80)
        glass_nodes = []
        for i, node in enumerate(data['nodes']):
            name = node.get('name', f'Node_{i}')
            if any(keyword in name.lower() for keyword in ['glass', 'window', 'windshield']):
                glass_nodes.append((i, name))
                print(f"[{i}] {name} (GLASS)")

        if glass_nodes:
            print(f"\nFound {len(glass_nodes)} glass-related nodes")

    # Meshes
    if "meshes" in data:
        print(f"\n\n{len(data['meshes'])} MESHES FOUND:")
        print("-" * 80)
        for i, mesh in enumerate(data['meshes']):
            name = mesh.get('name', f'Mesh_{i}')
            primitives = mesh.get('primitives', [])
            print(f"\n[{i}] {name} - {len(primitives)} primitives")

            for j, prim in enumerate(primitives):
                mat_idx = prim.get('material', -1)
                if mat_idx >= 0 and 'materials' in data:
                    mat_name = data['materials'][mat_idx].get('name', f'Mat_{mat_idx}')
                    print(f"    Primitive {j}: Material [{mat_idx}] {mat_name}")

if __name__ == "__main__":
    inspect_car_model()
