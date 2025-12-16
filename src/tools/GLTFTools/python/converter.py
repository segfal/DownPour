"""
GLTF/GLB to JSON Converter

Comprehensive converter that reads GLTF/GLB files (including large ones)
and extracts all relevant information to JSON format.
"""

import json
import sys
from pathlib import Path
from typing import Dict, Any, List, Optional

try:
    import pygltflib
except ImportError:
    print("Error: pygltflib is not installed")
    print("Install with: pip install pygltflib")
    sys.exit(1)


class GLTFConverter:
    """Convert GLTF/GLB files to comprehensive JSON representation"""

    def __init__(self, input_path: str):
        self.input_path = Path(input_path)
        self.gltf = None

    def load(self) -> None:
        """Load GLTF or GLB file"""
        print(f"Loading {self.input_path}...")

        if self.input_path.suffix.lower() == '.glb':
            self.gltf = pygltflib.GLTF2.load_binary(str(self.input_path))
        else:
            self.gltf = pygltflib.GLTF2.load(str(self.input_path))

        print(f"✓ Loaded GLTF file successfully")

    def extract_metadata(self) -> Dict[str, Any]:
        """Extract asset metadata"""
        if not self.gltf or not self.gltf.asset:
            return {}

        asset = self.gltf.asset
        metadata = {
            "version": asset.version,
        }

        if asset.generator:
            metadata["generator"] = asset.generator
        if asset.copyright:
            metadata["copyright"] = asset.copyright
        if asset.minVersion:
            metadata["minVersion"] = asset.minVersion

        return metadata

    def extract_scenes(self) -> List[Dict[str, Any]]:
        """Extract scene hierarchy"""
        if not self.gltf or not self.gltf.scenes:
            return []

        scenes = []
        for idx, scene in enumerate(self.gltf.scenes):
            scene_data = {
                "index": idx,
                "nodes": scene.nodes if scene.nodes else []
            }

            if scene.name:
                scene_data["name"] = scene.name

            scenes.append(scene_data)

        return scenes

    def extract_nodes(self) -> List[Dict[str, Any]]:
        """Extract all nodes with transforms (TRS or matrix)"""
        if not self.gltf or not self.gltf.nodes:
            return []

        nodes = []
        for idx, node in enumerate(self.gltf.nodes):
            node_data = {
                "index": idx,
            }

            if node.name:
                node_data["name"] = node.name

            if node.mesh is not None:
                node_data["mesh"] = node.mesh

            if node.camera is not None:
                node_data["camera"] = node.camera

            if node.skin is not None:
                node_data["skin"] = node.skin

            if node.children:
                node_data["children"] = node.children

            # Extract transform data
            if node.matrix:
                node_data["matrix"] = node.matrix

            # TRS components
            if node.translation:
                node_data["translation"] = node.translation

            if node.rotation:
                node_data["rotation"] = node.rotation

            if node.scale:
                node_data["scale"] = node.scale

            nodes.append(node_data)

        return nodes

    def extract_meshes(self) -> List[Dict[str, Any]]:
        """Extract mesh geometry information (primitive counts, attributes)"""
        if not self.gltf or not self.gltf.meshes:
            return []

        meshes = []
        for idx, mesh in enumerate(self.gltf.meshes):
            mesh_data = {
                "index": idx,
                "primitives": []
            }

            if mesh.name:
                mesh_data["name"] = mesh.name

            if mesh.weights:
                mesh_data["weights"] = mesh.weights

            # Extract primitives
            for prim_idx, prim in enumerate(mesh.primitives):
                prim_data = {
                    "index": prim_idx,
                    "mode": prim.mode if prim.mode is not None else 4,  # Default TRIANGLES
                    "attributes": {}
                }

                if prim.material is not None:
                    prim_data["material"] = prim.material

                # Extract attribute accessors
                if prim.attributes.POSITION is not None:
                    accessor = self.gltf.accessors[prim.attributes.POSITION]
                    prim_data["attributes"]["POSITION"] = {
                        "accessor": prim.attributes.POSITION,
                        "count": accessor.count,
                        "type": accessor.type,
                        "componentType": accessor.componentType
                    }

                if prim.attributes.NORMAL is not None:
                    accessor = self.gltf.accessors[prim.attributes.NORMAL]
                    prim_data["attributes"]["NORMAL"] = {
                        "accessor": prim.attributes.NORMAL,
                        "count": accessor.count,
                        "type": accessor.type,
                        "componentType": accessor.componentType
                    }

                if prim.attributes.TEXCOORD_0 is not None:
                    accessor = self.gltf.accessors[prim.attributes.TEXCOORD_0]
                    prim_data["attributes"]["TEXCOORD_0"] = {
                        "accessor": prim.attributes.TEXCOORD_0,
                        "count": accessor.count,
                        "type": accessor.type,
                        "componentType": accessor.componentType
                    }

                if prim.attributes.COLOR_0 is not None:
                    accessor = self.gltf.accessors[prim.attributes.COLOR_0]
                    prim_data["attributes"]["COLOR_0"] = {
                        "accessor": prim.attributes.COLOR_0,
                        "count": accessor.count,
                        "type": accessor.type,
                        "componentType": accessor.componentType
                    }

                # Extract indices
                if prim.indices is not None:
                    indices_accessor = self.gltf.accessors[prim.indices]
                    prim_data["indices"] = {
                        "accessor": prim.indices,
                        "count": indices_accessor.count,
                        "type": indices_accessor.type,
                        "componentType": indices_accessor.componentType
                    }

                mesh_data["primitives"].append(prim_data)

            meshes.append(mesh_data)

        return meshes

    def extract_materials(self) -> List[Dict[str, Any]]:
        """Extract PBR material properties (baseColor, metallic, roughness, textures)"""
        if not self.gltf or not self.gltf.materials:
            return []

        materials = []
        for idx, mat in enumerate(self.gltf.materials):
            mat_data = {
                "index": idx,
            }

            if mat.name:
                mat_data["name"] = mat.name

            # PBR Metallic Roughness
            if mat.pbrMetallicRoughness:
                pbr = mat.pbrMetallicRoughness
                mat_data["pbrMetallicRoughness"] = {}

                if pbr.baseColorFactor:
                    mat_data["pbrMetallicRoughness"]["baseColorFactor"] = pbr.baseColorFactor

                if pbr.metallicFactor is not None:
                    mat_data["pbrMetallicRoughness"]["metallicFactor"] = pbr.metallicFactor

                if pbr.roughnessFactor is not None:
                    mat_data["pbrMetallicRoughness"]["roughnessFactor"] = pbr.roughnessFactor

                if pbr.baseColorTexture:
                    mat_data["pbrMetallicRoughness"]["baseColorTexture"] = {
                        "index": pbr.baseColorTexture.index,
                        "texCoord": pbr.baseColorTexture.texCoord if pbr.baseColorTexture.texCoord is not None else 0
                    }

                if pbr.metallicRoughnessTexture:
                    mat_data["pbrMetallicRoughness"]["metallicRoughnessTexture"] = {
                        "index": pbr.metallicRoughnessTexture.index,
                        "texCoord": pbr.metallicRoughnessTexture.texCoord if pbr.metallicRoughnessTexture.texCoord is not None else 0
                    }

            # Normal texture
            if mat.normalTexture:
                mat_data["normalTexture"] = {
                    "index": mat.normalTexture.index,
                    "texCoord": mat.normalTexture.texCoord if mat.normalTexture.texCoord is not None else 0,
                    "scale": mat.normalTexture.scale if mat.normalTexture.scale is not None else 1.0
                }

            # Emissive
            if mat.emissiveFactor:
                mat_data["emissiveFactor"] = mat.emissiveFactor

            if mat.emissiveTexture:
                mat_data["emissiveTexture"] = {
                    "index": mat.emissiveTexture.index,
                    "texCoord": mat.emissiveTexture.texCoord if mat.emissiveTexture.texCoord is not None else 0
                }

            # Alpha mode
            if mat.alphaMode:
                mat_data["alphaMode"] = mat.alphaMode

            if mat.alphaCutoff is not None:
                mat_data["alphaCutoff"] = mat.alphaCutoff

            if mat.doubleSided is not None:
                mat_data["doubleSided"] = mat.doubleSided

            materials.append(mat_data)

        return materials

    def extract_textures(self) -> List[Dict[str, Any]]:
        """Extract texture information"""
        if not self.gltf or not self.gltf.textures:
            return []

        textures = []
        for idx, tex in enumerate(self.gltf.textures):
            tex_data = {
                "index": idx,
            }

            if tex.name:
                tex_data["name"] = tex.name

            if tex.source is not None:
                tex_data["source"] = tex.source

            if tex.sampler is not None:
                tex_data["sampler"] = tex.sampler

            textures.append(tex_data)

        return textures

    def extract_images(self) -> List[Dict[str, Any]]:
        """Extract image information"""
        if not self.gltf or not self.gltf.images:
            return []

        images = []
        for idx, img in enumerate(self.gltf.images):
            img_data = {
                "index": idx,
            }

            if img.name:
                img_data["name"] = img.name

            if img.uri:
                img_data["uri"] = img.uri

            if img.mimeType:
                img_data["mimeType"] = img.mimeType

            if img.bufferView is not None:
                img_data["bufferView"] = img.bufferView

            images.append(img_data)

        return images

    def extract_buffers(self) -> List[Dict[str, Any]]:
        """Extract buffer metadata (sizes, URIs, not full binary data)"""
        if not self.gltf or not self.gltf.buffers:
            return []

        buffers = []
        for idx, buf in enumerate(self.gltf.buffers):
            buf_data = {
                "index": idx,
                "byteLength": buf.byteLength
            }

            if hasattr(buf, 'uri') and buf.uri:
                buf_data["uri"] = buf.uri

            if hasattr(buf, 'name') and buf.name:
                buf_data["name"] = buf.name

            buffers.append(buf_data)

        return buffers

    def extract_accessors(self) -> List[Dict[str, Any]]:
        """Extract accessor information"""
        if not self.gltf or not self.gltf.accessors:
            return []

        accessors = []
        for idx, acc in enumerate(self.gltf.accessors):
            acc_data = {
                "index": idx,
                "componentType": acc.componentType,
                "count": acc.count,
                "type": acc.type
            }

            if acc.bufferView is not None:
                acc_data["bufferView"] = acc.bufferView

            if acc.byteOffset:
                acc_data["byteOffset"] = acc.byteOffset

            if acc.normalized is not None:
                acc_data["normalized"] = acc.normalized

            if acc.min:
                acc_data["min"] = acc.min

            if acc.max:
                acc_data["max"] = acc.max

            if acc.name:
                acc_data["name"] = acc.name

            accessors.append(acc_data)

        return accessors

    def convert_to_json(self, include_accessors: bool = True) -> Dict[str, Any]:
        """Generate comprehensive JSON representation"""
        print("Extracting GLTF data...")

        json_data = {
            "asset": self.extract_metadata(),
            "scenes": self.extract_scenes(),
            "nodes": self.extract_nodes(),
            "meshes": self.extract_meshes(),
            "materials": self.extract_materials(),
            "textures": self.extract_textures(),
            "images": self.extract_images(),
            "buffers": self.extract_buffers(),
        }

        if include_accessors:
            json_data["accessors"] = self.extract_accessors()

        # Add extensions if present
        if self.gltf.extensionsUsed:
            json_data["extensionsUsed"] = self.gltf.extensionsUsed

        if self.gltf.extensionsRequired:
            json_data["extensionsRequired"] = self.gltf.extensionsRequired

        print("✓ Extraction complete")
        return json_data

    def save_json(self, output_path: str, pretty: bool = True) -> None:
        """Save to JSON file"""
        json_data = self.convert_to_json()

        output_file = Path(output_path)
        output_file.parent.mkdir(parents=True, exist_ok=True)

        with open(output_file, 'w') as f:
            if pretty:
                json.dump(json_data, f, indent=2)
            else:
                json.dump(json_data, f)

        print(f"✓ Saved JSON to: {output_file}")
        print(f"  File size: {output_file.stat().st_size:,} bytes")

    @staticmethod
    def convert_file(input_path: str, output_path: str, pretty: bool = True) -> None:
        """Convenience method for one-shot conversion"""
        converter = GLTFConverter(input_path)
        converter.load()
        converter.save_json(output_path, pretty=pretty)


def main():
    """CLI entry point"""
    if len(sys.argv) < 2:
        print("GLTF/GLB to JSON Converter")
        print("")
        print("Usage:")
        print("  python converter.py <input.glb> [output.json]")
        print("")
        print("Examples:")
        print("  python converter.py model.glb")
        print("  python converter.py model.glb output.json")
        print("  python converter.py model.gltf model_data.json")
        print("")
        sys.exit(1)

    input_file = sys.argv[1]

    if len(sys.argv) >= 3:
        output_file = sys.argv[2]
    else:
        # Auto-generate output filename
        input_path = Path(input_file)
        output_file = str(input_path.with_suffix('.json'))

    try:
        GLTFConverter.convert_file(input_file, output_file)
        print("")
        print("✓ Conversion successful!")

    except FileNotFoundError:
        print(f"Error: Input file not found: {input_file}")
        sys.exit(1)
    except Exception as e:
        print(f"Error during conversion: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
