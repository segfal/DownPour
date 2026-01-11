#!/usr/bin/env python3
"""
Interactive Cockpit Position Calculator
Adjust percentages to find the perfect camera position
"""

import sys

# Model bounds
min_bounds = (-2.46315, -1.04289, -3.68455)
max_bounds = (2.3303, 1.04289, 0.994301)

width = max_bounds[0] - min_bounds[0]
height = max_bounds[1] - min_bounds[1]
depth = max_bounds[2] - min_bounds[2]

def calculate_position(x_percent, y_percent, z_percent):
    """Calculate position based on percentages"""
    x = min_bounds[0] + width * (x_percent / 100.0)
    y = min_bounds[1] + height * (y_percent / 100.0)
    z = min_bounds[2] + depth * (z_percent / 100.0)
    return x, y, z

print("=" * 60)
print("INTERACTIVE COCKPIT POSITION CALCULATOR")
print("=" * 60)
print(f"\nCar dimensions: {width:.2f}m (W) x {height:.2f}m (H) x {depth:.2f}m (D)")
print("\nEnter percentages (0-100) or press Enter to use defaults:")
print("  X: 0% = left side, 100% = right side")
print("  Y: 0% = ground, 100% = roof")
print("  Z: 0% = rear bumper, 100% = front bumper")

try:
    x_input = input("\nX percent [35]: ").strip()
    x_percent = float(x_input) if x_input else 35.0
    
    y_input = input("Y percent [55]: ").strip()
    y_percent = float(y_input) if y_input else 55.0
    
    z_input = input("Z percent [60]: ").strip()
    z_percent = float(z_input) if z_input else 60.0
    
    x, y, z = calculate_position(x_percent, y_percent, z_percent)
    
    print("\n" + "=" * 60)
    print("CALCULATED POSITION")
    print("=" * 60)
    print(f"\nPercentages: X={x_percent}%, Y={y_percent}%, Z={z_percent}%")
    print(f"Position: ({x:.6f}, {y:.6f}, {z:.6f})")
    
    print("\n" + "=" * 60)
    print("C++ CODE TO COPY")
    print("=" * 60)
    print(f"\ncockpitOffset.x = minBounds.x + dimensions.x * {x_percent/100:.2f}f;")
    print(f"cockpitOffset.y = minBounds.y + dimensions.y * {y_percent/100:.2f}f;")
    print(f"cockpitOffset.z = minBounds.z + dimensions.z * {z_percent/100:.2f}f;")
    
    print("\nOr directly:")
    print(f"glm::vec3 cockpitOffset({x:.6f}f, {y:.6f}f, {z:.6f}f);")
    
    # Provide guidance
    print("\n" + "=" * 60)
    print("SUGGESTIONS")
    print("=" * 60)
    if z_percent > 70:
        print("⚠️  Camera might be too far forward (near windshield)")
    elif z_percent < 50:
        print("⚠️  Camera might be in the back seats")
    else:
        print("✓ Z position looks good for driver's seat")
    
    if y_percent > 70:
        print("⚠️  Camera might be too high (near roof)")
    elif y_percent < 40:
        print("⚠️  Camera might be too low")
    else:
        print("✓ Y position looks good for eye level")
    
    if x_percent < 20 or x_percent > 80:
        print("⚠️  Camera might be too far to one side")
    else:
        print("✓ X position looks reasonable")
    
except ValueError:
    print("\nError: Please enter valid numbers")
    sys.exit(1)
except KeyboardInterrupt:
    print("\n\nCancelled.")
    sys.exit(0)
