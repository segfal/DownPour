#!/usr/bin/env python3
"""
Skybox Color Generator
Generate different sky colors for daylight scenes
"""

import colorsys

def rgb_to_glsl(r, g, b):
    """Convert 0-255 RGB to 0-1 GLSL format"""
    return (r/255.0, g/255.0, b/255.0)

print("=" * 60)
print("SKYBOX COLOR PRESETS")
print("=" * 60)

presets = {
    "Clear Blue Sky": {
        "top": (135, 206, 235),      # Sky blue
        "horizon": (225, 235, 245),   # Light blue
        "description": "Bright sunny day"
    },
    "Sunset": {
        "top": (255, 140, 60),        # Orange
        "horizon": (255, 200, 150),   # Light orange
        "description": "Golden hour"
    },
    "Overcast": {
        "top": (180, 180, 190),       # Gray
        "horizon": (200, 200, 210),   # Light gray
        "description": "Cloudy weather"
    },
    "Dawn": {
        "top": (255, 180, 180),       # Pink
        "horizon": (255, 220, 200),   # Light pink
        "description": "Early morning"
    },
    "Night": {
        "top": (10, 10, 30),          # Dark blue
        "horizon": (30, 30, 60),      # Darker blue
        "description": "Nighttime"
    },
    "Rainy Day": {
        "top": (120, 130, 140),       # Dark gray
        "horizon": (160, 170, 180),   # Medium gray
        "description": "Rainy weather (for your rain sim!)"
    }
}

for name, colors in presets.items():
    top_rgb = colors["top"]
    horizon_rgb = colors["horizon"]
    top_gl = rgb_to_glsl(*top_rgb)
    horizon_gl = rgb_to_glsl(*horizon_rgb)
    
    print(f"\n{name}: {colors['description']}")
    print(f"  Top:     RGB{top_rgb} → vec3({top_gl[0]:.3f}, {top_gl[1]:.3f}, {top_gl[2]:.3f})")
    print(f"  Horizon: RGB{horizon_rgb} → vec3({horizon_gl[0]:.3f}, {horizon_gl[1]:.3f}, {horizon_gl[2]:.3f})")

print("\n" + "=" * 60)
print("SHADER CODE EXAMPLES")
print("=" * 60)

print("\n// In your skybox fragment shader:")
print("// Mix between top and horizon based on Y coordinate")
print("""
vec3 topColor = vec3(0.529, 0.808, 0.922);    // Clear blue
vec3 horizonColor = vec3(0.882, 0.922, 0.961); // Light blue

// Assuming fragTexCoord.y goes from 0 (bottom) to 1 (top)
vec3 skyColor = mix(horizonColor, topColor, fragTexCoord.y);
outColor = vec4(skyColor, 1.0);
""")

print("\n// For rainy day atmosphere:")
print("""
vec3 topColor = vec3(0.471, 0.510, 0.549);     // Dark gray
vec3 horizonColor = vec3(0.627, 0.667, 0.706); // Medium gray

vec3 skyColor = mix(horizonColor, topColor, fragTexCoord.y);
outColor = vec4(skyColor, 1.0);
""")

print("\n" + "=" * 60)
print("CUSTOM COLOR GENERATOR")
print("=" * 60)

try:
    print("\nEnter RGB values (0-255) for custom sky color:")
    r = int(input("Red [135]: ").strip() or "135")
    g = int(input("Green [206]: ").strip() or "206")
    b = int(input("Blue [235]: ").strip() or "235")
    
    gl_r, gl_g, gl_b = rgb_to_glsl(r, g, b)
    print(f"\nRGB({r}, {g}, {b})")
    print(f"GLSL: vec3({gl_r:.3f}f, {gl_g:.3f}f, {gl_b:.3f}f)")
    
except (ValueError, KeyboardInterrupt):
    print("\nUsing defaults...")
