#!/usr/bin/env python3
"""
Model Bounds Visualizer
Shows the car model bounds and suggests camera positions
"""

# Model bounds from your BMW M4
min_bounds = (-2.46315, -1.04289, -3.68455)
max_bounds = (2.3303, 1.04289, 0.994301)

# Calculate dimensions
width = max_bounds[0] - min_bounds[0]
height = max_bounds[1] - min_bounds[1]
depth = max_bounds[2] - min_bounds[2]

print("=" * 50)
print("BMW M4 MODEL INFORMATION")
print("=" * 50)
print(f"\nDimensions:")
print(f"  Width:  {width:.2f}m")
print(f"  Height: {height:.2f}m")
print(f"  Depth:  {depth:.2f}m")

print(f"\nBounds:")
print(f"  Min: ({min_bounds[0]:.2f}, {min_bounds[1]:.2f}, {min_bounds[2]:.2f})")
print(f"  Max: ({max_bounds[0]:.2f}, {max_bounds[1]:.2f}, {max_bounds[2]:.2f})")

center_x = (min_bounds[0] + max_bounds[0]) / 2
center_y = (min_bounds[1] + min_bounds[1]) / 2
center_z = (min_bounds[2] + max_bounds[2]) / 2

print(f"\nCenter: ({center_x:.2f}, {center_y:.2f}, {center_z:.2f})")

print("\n" + "=" * 50)
print("SUGGESTED CAMERA POSITIONS")
print("=" * 50)

# Different viewing positions
positions = {
    "Cockpit View (Driver)": {
        "x": min_bounds[0] + width * 0.35,
        "y": min_bounds[1] + height * 0.55,
        "z": min_bounds[2] + depth * 0.60,
        "description": "Inside car, driver's seat"
    },
    "Cockpit View (Centered)": {
        "x": center_x,
        "y": min_bounds[1] + height * 0.55,
        "z": min_bounds[2] + depth * 0.60,
        "description": "Inside car, centered"
    },
    "Front View": {
        "x": center_x,
        "y": center_y,
        "z": max_bounds[2] + 5.0,
        "description": "5m in front of car"
    },
    "Side View (Left)": {
        "x": min_bounds[0] - 5.0,
        "y": center_y,
        "z": center_z,
        "description": "5m to the left"
    },
    "Side View (Right)": {
        "x": max_bounds[0] + 5.0,
        "y": center_y,
        "z": center_z,
        "description": "5m to the right"
    },
    "Top View": {
        "x": center_x,
        "y": max_bounds[1] + 10.0,
        "z": center_z,
        "description": "10m above car"
    },
    "Rear View": {
        "x": center_x,
        "y": center_y,
        "z": min_bounds[2] - 5.0,
        "description": "5m behind car"
    }
}

for name, pos in positions.items():
    print(f"\n{name}:")
    print(f"  Position: ({pos['x']:.2f}, {pos['y']:.2f}, {pos['z']:.2f})")
    print(f"  {pos['description']}")

print("\n" + "=" * 50)
print("COPY-PASTE VALUES FOR C++ CODE")
print("=" * 50)
print("\nCurrent cockpit offset (driver's seat):")
cockpit_x = min_bounds[0] + width * 0.35
cockpit_y = min_bounds[1] + height * 0.55
cockpit_z = min_bounds[2] + depth * 0.60
print(f"glm::vec3 cockpitOffset({cockpit_x:.6f}f, {cockpit_y:.6f}f, {cockpit_z:.6f}f);")

print("\nTry adjusting percentages:")
print("// X: 0.35 = 35% from left (0.0 = left edge, 1.0 = right edge)")
print("// Y: 0.55 = 55% height (0.0 = ground, 1.0 = roof)")
print("// Z: 0.60 = 60% from rear (0.0 = rear bumper, 1.0 = front bumper)")
