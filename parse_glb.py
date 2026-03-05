#!/usr/bin/env python3
"""
Parse a GLB (binary glTF) file and extract:
1. All node names in the scene hierarchy
2. Mesh bounding box (accessor min/max for POSITION attributes)
3. Scene hierarchy (parent-child relationships)
"""

import struct
import json
import sys

GLB_MAGIC = 0x46546C67  # 'glTF'
CHUNK_TYPE_JSON = 0x4E4F534A  # 'JSON'
CHUNK_TYPE_BIN = 0x004E4942   # 'BIN\0'


def parse_glb_header(data):
    """Parse the 12-byte GLB header."""
    magic, version, length = struct.unpack_from('<III', data, 0)
    if magic != GLB_MAGIC:
        raise ValueError(f"Not a valid GLB file. Magic: 0x{magic:08X}, expected 0x{GLB_MAGIC:08X}")
    return version, length


def parse_chunks(data):
    """Parse GLB chunks after the 12-byte header."""
    offset = 12
    chunks = []
    while offset < len(data):
        chunk_length, chunk_type = struct.unpack_from('<II', data, offset)
        offset += 8
        chunk_data = data[offset:offset + chunk_length]
        chunks.append((chunk_type, chunk_data))
        offset += chunk_length
    return chunks


def print_hierarchy(gltf, node_index, indent=0, parent_name="(root)"):
    """Recursively print the node hierarchy."""
    node = gltf['nodes'][node_index]
    name = node.get('name', f'<unnamed_{node_index}>')
    mesh_info = ""
    if 'mesh' in node:
        mesh_idx = node['mesh']
        mesh = gltf['meshes'][mesh_idx]
        mesh_name = mesh.get('name', f'mesh_{mesh_idx}')
        mesh_info = f"  [mesh: {mesh_name}, primitives: {len(mesh.get('primitives', []))}]"

    transform_info = ""
    if 'translation' in node:
        t = node['translation']
        transform_info += f"  T=[{t[0]:.3f}, {t[1]:.3f}, {t[2]:.3f}]"
    if 'rotation' in node:
        r = node['rotation']
        transform_info += f"  R=[{r[0]:.3f}, {r[1]:.3f}, {r[2]:.3f}, {r[3]:.3f}]"
    if 'scale' in node:
        s = node['scale']
        transform_info += f"  S=[{s[0]:.3f}, {s[1]:.3f}, {s[2]:.3f}]"

    prefix = "  " * indent
    print(f"{prefix}[{node_index}] {name}{mesh_info}{transform_info}")

    children = node.get('children', [])
    for child_idx in children:
        print_hierarchy(gltf, child_idx, indent + 1, name)


def collect_position_accessors(gltf):
    """Find all accessor indices used for POSITION attributes in mesh primitives."""
    position_accessors = []
    for mesh_idx, mesh in enumerate(gltf.get('meshes', [])):
        mesh_name = mesh.get('name', f'mesh_{mesh_idx}')
        for prim_idx, prim in enumerate(mesh.get('primitives', [])):
            attrs = prim.get('attributes', {})
            if 'POSITION' in attrs:
                acc_idx = attrs['POSITION']
                position_accessors.append((mesh_name, prim_idx, acc_idx))
    return position_accessors


def main():
    filepath = "/Users/adminh/DownPour/assets/models/bmw_suv.glb"

    print(f"Parsing GLB file: {filepath}")
    print("=" * 80)

    with open(filepath, 'rb') as f:
        data = f.read()

    version, total_length = parse_glb_header(data)
    print(f"GLB Version: {version}")
    print(f"Total file size: {total_length:,} bytes ({total_length / 1024 / 1024:.2f} MB)")
    print()

    chunks = parse_chunks(data)
    print(f"Number of chunks: {len(chunks)}")
    for i, (ctype, cdata) in enumerate(chunks):
        type_str = struct.pack('<I', ctype).decode('ascii', errors='replace')
        print(f"  Chunk {i}: type=0x{ctype:08X} ({type_str}), size={len(cdata):,} bytes")
    print()

    # Extract JSON chunk
    json_chunk = None
    for ctype, cdata in chunks:
        if ctype == CHUNK_TYPE_JSON:
            json_chunk = cdata
            break

    if json_chunk is None:
        print("ERROR: No JSON chunk found in GLB file.")
        sys.exit(1)

    gltf = json.loads(json_chunk.decode('utf-8'))

    # =========================================================================
    # 1. Print all node names
    # =========================================================================
    print("=" * 80)
    print("ALL NODES")
    print("=" * 80)
    nodes = gltf.get('nodes', [])
    print(f"Total nodes: {len(nodes)}")
    print()
    for i, node in enumerate(nodes):
        name = node.get('name', '<unnamed>')
        has_mesh = 'mesh' in node
        children_count = len(node.get('children', []))
        extras = ""
        if has_mesh:
            extras += f" [has mesh #{node['mesh']}]"
        if children_count > 0:
            extras += f" [children: {children_count}]"
        print(f"  Node {i:3d}: {name}{extras}")

    # =========================================================================
    # 2. Print mesh bounding boxes from accessor min/max
    # =========================================================================
    print()
    print("=" * 80)
    print("MESH BOUNDING BOXES (from POSITION accessor min/max)")
    print("=" * 80)

    position_accessors = collect_position_accessors(gltf)
    accessors = gltf.get('accessors', [])

    global_min = [float('inf')] * 3
    global_max = [float('-inf')] * 3

    for mesh_name, prim_idx, acc_idx in position_accessors:
        accessor = accessors[acc_idx]
        acc_min = accessor.get('min')
        acc_max = accessor.get('max')
        count = accessor.get('count', 0)

        print(f"\n  Mesh: {mesh_name} (primitive {prim_idx}, accessor {acc_idx})")
        print(f"    Vertex count: {count}")
        if acc_min and acc_max:
            print(f"    Min: [{acc_min[0]:.6f}, {acc_min[1]:.6f}, {acc_min[2]:.6f}]")
            print(f"    Max: [{acc_max[0]:.6f}, {acc_max[1]:.6f}, {acc_max[2]:.6f}]")
            dx = acc_max[0] - acc_min[0]
            dy = acc_max[1] - acc_min[1]
            dz = acc_max[2] - acc_min[2]
            print(f"    Size: [{dx:.6f}, {dy:.6f}, {dz:.6f}]")

            # Update global bounds
            for j in range(3):
                if acc_min[j] < global_min[j]:
                    global_min[j] = acc_min[j]
                if acc_max[j] > global_max[j]:
                    global_max[j] = acc_max[j]
        else:
            print("    (no min/max available on accessor)")

    if global_min[0] != float('inf'):
        print()
        print("-" * 60)
        print("GLOBAL BOUNDING BOX (union of all mesh POSITION accessors):")
        print(f"  Min: [{global_min[0]:.6f}, {global_min[1]:.6f}, {global_min[2]:.6f}]")
        print(f"  Max: [{global_max[0]:.6f}, {global_max[1]:.6f}, {global_max[2]:.6f}]")
        gx = global_max[0] - global_min[0]
        gy = global_max[1] - global_min[1]
        gz = global_max[2] - global_min[2]
        print(f"  Overall dimensions: {gx:.6f} x {gy:.6f} x {gz:.6f}")
        print(f"  (width x height x depth, assuming X=right, Y=up, Z=forward)")

    # =========================================================================
    # 3. Print scene hierarchy
    # =========================================================================
    print()
    print("=" * 80)
    print("SCENE HIERARCHY")
    print("=" * 80)

    scenes = gltf.get('scenes', [])
    default_scene = gltf.get('scene', 0)
    print(f"Number of scenes: {len(scenes)}, default scene: {default_scene}")

    for scene_idx, scene in enumerate(scenes):
        scene_name = scene.get('name', f'scene_{scene_idx}')
        root_nodes = scene.get('nodes', [])
        marker = " (DEFAULT)" if scene_idx == default_scene else ""
        print(f"\nScene {scene_idx}: \"{scene_name}\"{marker}")
        print(f"  Root nodes: {root_nodes}")
        print()
        for root_idx in root_nodes:
            print_hierarchy(gltf, root_idx, indent=1)

    # =========================================================================
    # Additional info: materials and meshes summary
    # =========================================================================
    print()
    print("=" * 80)
    print("MATERIALS SUMMARY")
    print("=" * 80)
    materials = gltf.get('materials', [])
    print(f"Total materials: {len(materials)}")
    for i, mat in enumerate(materials):
        name = mat.get('name', '<unnamed>')
        alpha_mode = mat.get('alphaMode', 'OPAQUE')
        double_sided = mat.get('doubleSided', False)
        pbr = mat.get('pbrMetallicRoughness', {})
        has_base_tex = 'baseColorTexture' in pbr
        has_normal = 'normalTexture' in mat
        has_mr = 'metallicRoughnessTexture' in pbr
        has_emissive = 'emissiveTexture' in mat

        textures = []
        if has_base_tex:
            textures.append("baseColor")
        if has_normal:
            textures.append("normal")
        if has_mr:
            textures.append("metalRough")
        if has_emissive:
            textures.append("emissive")

        tex_str = ", ".join(textures) if textures else "none"
        print(f"  [{i}] {name} | alpha={alpha_mode} | doubleSided={double_sided} | textures: {tex_str}")

    print()
    print("=" * 80)
    print("MESHES SUMMARY")
    print("=" * 80)
    meshes = gltf.get('meshes', [])
    print(f"Total meshes: {len(meshes)}")
    for i, mesh in enumerate(meshes):
        name = mesh.get('name', '<unnamed>')
        prims = mesh.get('primitives', [])
        mat_indices = [p.get('material', -1) for p in prims]
        mat_names = []
        for mi in mat_indices:
            if 0 <= mi < len(materials):
                mat_names.append(materials[mi].get('name', f'mat_{mi}'))
            else:
                mat_names.append('(none)')
        print(f"  [{i}] {name} | primitives: {len(prims)} | materials: {mat_names}")

    print()
    print("Done.")


if __name__ == '__main__':
    main()
