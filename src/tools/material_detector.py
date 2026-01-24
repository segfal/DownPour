#load gtlf model
from pygltflib import GLTF2
import json



# Load the GLB file
gltf = GLTF2().load('models/bmw.glb')



material_data = []
for idx, material in enumerate(gltf.materials):
    material_data.append({
        'name': material.name,
        'pbrMetallicRoughness': {
            'baseColorFactor': material.pbrMetallicRoughness.baseColorFactor,
            'metallicFactor': material.pbrMetallicRoughness.metallicFactor,
            'roughnessFactor': material.pbrMetallicRoughness.roughnessFactor,
        }
    })
    if material.pbrMetallicRoughness.baseColorTexture:
        material_data[-1]['pbrMetallicRoughness']['baseColorTexture'] = material.pbrMetallicRoughness.baseColorTexture.index
    if material.pbrMetallicRoughness.metallicRoughnessTexture:
        material_data[-1]['pbrMetallicRoughness']['metallicRoughnessTexture'] = material.pbrMetallicRoughness.metallicRoughnessTexture.index
    if material.normalTexture:
        material_data[-1]['normalTexture'] = material.normalTexture.index
    if material.emissiveTexture:
        material_data[-1]['emissiveTexture'] = material.emissiveTexture.index

    
with open('material_data.json', 'w') as f:
    json.dump(material_data, f, indent=4)
