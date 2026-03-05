#version 450

// ============================================================================
// Screen Rain Fragment Shader — Procedural Rain Drops on Camera
//
// Three layers of animated rain droplets composited onto the screen:
//   Layer 1: Stationary drops that appear, sit, then slowly slide down
//   Layer 2: Sliding streaks — larger drops that travel down fast
//   Layer 3: Splash impacts — brief expanding rings (heavy/severe only)
//
// All procedural — no textures needed. Uses cell-based grids with
// hash randomization for deterministic but natural-looking patterns.
// ============================================================================

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    mat4 invViewProjection;  // inverse(viewProjection) for depth-to-world reconstruction
    vec4 sunDirection;
    vec4 cameraPosition;   // w = elapsed time
    vec4 weatherParams;    // x = rainIntensity, y = wetness, z = windX, w = windZ
} camera;

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

// ============================================================================
// Hash functions
// ============================================================================

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

vec2 hash2(vec2 p) {
    return vec2(
        fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453),
        fract(sin(dot(p, vec2(269.5, 183.3))) * 43758.5453)
    );
}

// ============================================================================
// Smooth drop shape — soft circle with configurable aspect ratio
// ============================================================================
float dropShape(vec2 uv, vec2 center, float radius, float aspectY) {
    vec2 d = uv - center;
    d.y /= aspectY;  // Stretch vertically for elongated drops
    float dist = length(d);
    return smoothstep(radius, radius * 0.3, dist);
}

// ============================================================================
// Layer 1: Stationary Drops
//
// Small droplets that appear on the screen, sit briefly, then slide
// downward under gravity. Sparse at low rain, denser at high.
// ============================================================================
float stationaryDrops(vec2 uv, float time, float intensity, float windBias) {
    float total = 0.0;

    // Two sub-layers at different grid scales for variety
    for (int layer = 0; layer < 2; layer++) {
        vec2 gridSize = (layer == 0) ? vec2(30.0, 20.0) : vec2(20.0, 14.0);
        float speed   = (layer == 0) ? 0.8 : 0.6;
        float maxAlpha = (layer == 0) ? 0.35 : 0.25;

        vec2 cellUV = uv * gridSize;
        vec2 cell = floor(cellUV);
        vec2 f = fract(cellUV);

        // Per-cell random values
        float cellRand = hash(cell + float(layer) * 47.0);
        vec2 dropCenter = hash2(cell + float(layer) * 13.7);

        // Only spawn drop if random < intensity threshold
        // More drops appear as rain gets heavier
        float threshold = mix(0.95, 0.3, intensity);
        if (cellRand > threshold) continue;

        // Drop lifecycle: spawn → sit → slide → vanish
        float cycleLength = 1.5 + cellRand * 2.0;  // 1.5–3.5 seconds per cycle
        float phase = fract(time * speed / cycleLength + cellRand);

        // Slide offset (gravity pull + wind)
        float slideY = smoothstep(0.2, 1.0, phase) * 0.6;
        float slideX = windBias * smoothstep(0.1, 1.0, phase) * 0.15;

        vec2 pos = dropCenter * 0.7 + 0.15;  // Keep drops away from cell edges
        pos.y += slideY;
        pos.x += slideX;

        // Fade in quickly, fade out slowly
        float fadeIn  = smoothstep(0.0, 0.08, phase);
        float fadeOut = 1.0 - smoothstep(0.7, 1.0, phase);
        float alpha = fadeIn * fadeOut;

        // Drop size varies per cell, grows slightly as it slides
        float baseSize = 0.08 + cellRand * 0.12;
        float size = baseSize * (1.0 + slideY * 0.3);

        float drop = dropShape(f, pos, size, 1.3 + slideY * 0.5);
        total += drop * alpha * maxAlpha;
    }

    return total;
}

// ============================================================================
// Layer 2: Sliding Streaks
//
// Larger drops that travel down the screen quickly, leaving a slight
// trail. More visible during heavy/severe rain.
// ============================================================================
float slidingStreaks(vec2 uv, float time, float intensity, float windBias) {
    if (intensity < 0.2) return 0.0;  // Only visible in real rain

    float total = 0.0;
    float effectIntensity = smoothstep(0.2, 0.8, intensity);

    vec2 gridSize = vec2(12.0, 6.0);
    vec2 cellUV = uv * gridSize;
    vec2 cell = floor(cellUV);
    vec2 f = fract(cellUV);

    float cellRand = hash(cell + 91.0);
    vec2 dropCenter = hash2(cell + 23.5);

    // Fewer streaks — only the bigger drops
    float threshold = mix(0.9, 0.45, effectIntensity);
    if (cellRand > threshold) return 0.0;

    // Faster cycle, continuous sliding
    float speed = 1.2 + cellRand * 0.8;
    float phase = fract(time * speed * 0.4 + cellRand);

    // Strong downward slide + wind
    float slideY = phase * 1.8;
    float slideX = windBias * phase * 0.25;

    vec2 pos = dropCenter * 0.6 + 0.2;
    pos.y += slideY;
    pos.x += slideX;

    // Wrap around Y
    pos.y = fract(pos.y);

    // Fade
    float fadeIn  = smoothstep(0.0, 0.05, phase);
    float fadeOut = 1.0 - smoothstep(0.85, 1.0, phase);
    float alpha = fadeIn * fadeOut;

    // Elongated drop (tall ellipse for streak effect)
    float size = 0.06 + cellRand * 0.08;
    float drop = dropShape(f, pos, size, 2.5);

    // Trail behind the drop (stretched upward)
    vec2 trailPos = pos;
    trailPos.y -= 0.12;  // Trail above the drop (it's sliding down)
    float trail = dropShape(f, trailPos, size * 0.6, 4.0) * 0.3;

    total = (drop + trail) * alpha * 0.3 * effectIntensity;
    return total;
}

// ============================================================================
// Layer 3: Splash Impacts
//
// Brief expanding rings where drops hit the camera. Short-lived
// burst effect. Only active during heavy/severe rain.
// ============================================================================
float splashImpacts(vec2 uv, float time, float intensity) {
    if (intensity < 0.4) return 0.0;  // Heavy+ only

    float total = 0.0;
    float effectIntensity = smoothstep(0.4, 1.0, intensity);

    vec2 gridSize = vec2(18.0, 12.0);
    vec2 cellUV = uv * gridSize;
    vec2 cell = floor(cellUV);
    vec2 f = fract(cellUV);

    float cellRand = hash(cell + 173.0);

    // Sparse splashes
    float threshold = mix(0.92, 0.6, effectIntensity);
    if (cellRand > threshold) return 0.0;

    vec2 center = hash2(cell + 57.3) * 0.6 + 0.2;

    // Very short cycle (0.3–0.6 seconds)
    float cycleLength = 0.3 + cellRand * 0.3;
    float phase = fract(time / cycleLength + cellRand);

    // Expanding ring
    float ringRadius = phase * 0.35;
    float ringWidth = 0.02 + 0.01 * (1.0 - phase);  // Thins as it expands
    float dist = length(f - center);

    float ring = smoothstep(ringRadius - ringWidth, ringRadius, dist)
               * smoothstep(ringRadius + ringWidth, ringRadius, dist);

    // Fade out quickly (splash is brief)
    float fade = (1.0 - phase) * (1.0 - phase);

    // Center flash at impact moment
    float flash = smoothstep(0.1, 0.0, phase) * smoothstep(0.08, 0.0, dist) * 0.5;

    total = (ring * fade + flash) * 0.25 * effectIntensity;
    return total;
}

// ============================================================================
// Main
// ============================================================================
void main() {
    float rainIntensity = camera.weatherParams.x;

    // Early out: no effect when sunny
    if (rainIntensity < 0.01) {
        discard;
    }

    float time = camera.cameraPosition.w;
    float windX = camera.weatherParams.z;

    // Normalize wind to [-1, 1] range for screen-space bias (wind is 0-6000 at 1000x scale)
    float windBias = clamp(windX / 6000.0, -1.0, 1.0);

    // Accumulate all three layers
    float drops   = stationaryDrops(fragUV, time, rainIntensity, windBias);
    float streaks = slidingStreaks(fragUV, time, rainIntensity, windBias);
    float splash  = splashImpacts(fragUV, time, rainIntensity);

    float totalEffect = drops + streaks + splash;

    // Nothing to draw
    if (totalEffect < 0.005) {
        discard;
    }

    // Composite: water darkens scene slightly, bright specular highlight on top
    // Water tint color (slightly blue-gray)
    vec3 waterTint = vec3(0.65, 0.70, 0.80);

    // Specular highlight (bright spot simulating light refraction through drop)
    // Offset toward sun direction projected to screen
    vec3 sunDir = normalize(camera.sunDirection.xyz);
    float specularStrength = max(sunDir.y, 0.1) * (1.0 - rainIntensity * 0.5);
    vec3 highlightColor = vec3(1.0, 0.98, 0.95) * specularStrength;

    // Lens-bulge: each drop acts as a tiny convex lens.
    // The highlight shifts toward the screen-space projection of the sun,
    // simulating the bright caustic spot you see at the top of a real raindrop.
    // We approximate this by brightening the edge of each drop that faces the sun.
    vec2 sunScreen = normalize(sunDir.xy + vec2(0.001));  // sun projected to 2D
    // Edge brightening: stronger where fragUV aligns with the sun projection
    float edgeHighlight = dot(normalize(fragUV - vec2(0.5)), sunScreen);
    edgeHighlight = max(edgeHighlight, 0.0);
    // Apply bulge specular only where drops are present
    vec3 bulgeHighlight = vec3(1.0, 0.99, 0.97) * edgeHighlight * drops * 0.5;

    // Mix dark water tint with bright highlight + lens-bulge
    float highlightMask = drops * 0.4 + streaks * 0.3;
    vec3 finalColor = mix(waterTint, highlightColor, highlightMask * 0.5) + bulgeHighlight;

    // Alpha: stronger effect = more visible overlay
    float alpha = clamp(totalEffect, 0.0, 0.6);

    outColor = vec4(finalColor, alpha);
}
