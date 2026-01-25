#load gtlf model
from pygltflib import GLTF2
import json

glb_paths = ['bmw.glb', 'road.glb']



class MaterialDetector:
    def __init__(self, glb_path):
        self.glb_path = glb_path
        self.glb_model = GLTF2().load(self.glb_path)
        self.materials = [material.name for material in self.glb_model.materials]
        self.save_material_data()
    def convert_path(self, path):
        return f'material_data/{path.split("/")[-1].split(".")[0]}.json'
    def save_material_data(self):
        with open(self.convert_path(self.glb_path), 'w') as f:
            json.dump(self.materials, f, indent=4)

    
for glb_path in glb_paths:
    MaterialDetector(f'models/{glb_path}')
