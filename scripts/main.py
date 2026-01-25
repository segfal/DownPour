import json

with open("assets/models/bmw/bmw.gltf.json", "r") as f:
    data = json.load(f)

print(*[data["nodes"][i]["name"] for i in range(len(data["nodes"])) if "name" in data["nodes"][i]], sep="\n")