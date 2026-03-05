#version 450

// ============================================================================
// Windshield Fragment Shader
//
// Procedural rain drops + wiper arc clearing + scene refraction.
//
// Algorithm:
//   1. Divide windshield UV into a ~20×15 grid of cells
//   2. Each cell hashes to a deterministic drop position + phase offset
//   3. Rain intensity gates how many cells spawn a drop
//   4. Drops accumulate (wetness grows with age), then slide down under gravity
//   5. Wiper arc: drops within the swept region are instantly cleared
//   6. Each drop acts as a tiny lens: distort fragScreenUV radially
//   7. Sample scene behind glass through the distorted UV (real refraction)
//   8. Composite: glass tint + refraction + specular highlights
// ============================================================================

const float PI = 3.14159265359;

// ----------------------------------------------------------------
// Descriptor sets
// ----------------------------------------------------------------
layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    mat4 invViewProjection;  // inverse(viewProjection) for depth-to-world reconstruction
    vec4 sunDirection;
    vec4 cameraPosition;   // w = elapsed time
    vec4 weatherParams;    // x = rainIntensity, y = wetness, z = windX, w = windZ
} camera;

layout(set = 1, binding = 0) uniform sampler2D baseColorMap;
layout(set = 1, binding = 1) uniform sampler2D normalMap;
layout(set = 1, binding = 2) uniform sampler2D metallicRoughnessMap;

// Set 2: scene behind the glass (snapshot from previous render)
layout(set = 2, binding = 0) uniform sampler2D sceneSnapshot;

// ----------------------------------------------------------------
// Push constants
// ----------------------------------------------------------------
layout(push_constant) uniform PushConstants {
    mat4  model;
    float alphaMultiplier;
    float wiperAngle;     // degrees, -45..+45
    float rainIntensity;
    float carSpeed;
    float time;
} push;

// ----------------------------------------------------------------
// Fragment inputs
// ----------------------------------------------------------------
layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragWorldPos;
layout(location = 3) in float fragDistance;
layout(location = 4) in vec2 fragScreenUV;

layout(location = 0) out vec4 outColor;

// ----------------------------------------------------------------
// Hash helpers (same as screen_rain.frag)
// ----------------------------------------------------------------
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}
vec2 hash2(vec2 p) {
    return vec2(
        fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453),
        fract(sin(dot(p, vec2(269.5, 183.3))) * 43758.5453)
    );
}

// ----------------------------------------------------------------
// Main
// ----------------------------------------------------------------
void main() {
    float rainIntensity = push.rainIntensity;
    float time          = push.time;
    float carSpeed      = push.carSpeed;
    float windX         = camera.weatherParams.z;
    float wiperAngleDeg = push.wiperAngle;

    // ----------------------------------------------------------------
    // 1. Wiper arc mask (UV-space arc sweep)
    //
    // Wiper pivot sits below the windshield UV space (at v ≈ 1.3).
    // The wiper sweeps an arc from -45° to +45° around the pivot.
    // Any UV point whose angle from the pivot matches the current wiper
    // angle is within the cleared arc.
    // ----------------------------------------------------------------
    const vec2  wiperPivot    = vec2(0.5, 1.25);  // UV pivot, below glass
    const float bladeHalfArc  = 0.04;              // angular half-width of blade (radians)

    vec2  toFrag     = fragTexCoord - wiperPivot;
    float fragAngle  = atan(toFrag.x, -toFrag.y);  // angle from pivot (radians)
    float wiperRad   = wiperAngleDeg * PI / 180.0;

    // Distance from wiper arm to this fragment's angle
    float angleDist  = abs(fragAngle - wiperRad);
    // Also account for radial extent (wiper only reaches so far)
    float radDist    = length(toFrag);
    bool  inWiperArc = (angleDist < bladeHalfArc) && (radDist > 0.05) && (radDist < 1.3);

    // ----------------------------------------------------------------
    // 2. Droplet grid (20×15 cells)
    // ----------------------------------------------------------------
    const vec2  gridSize  = vec2(20.0, 15.0);   // Larger cells = bigger, more visible drops
    const float growSpeed = 0.3;   // wetness growth rate (cycles per second)
    const float gravity   = 0.08;  // UV units per second slide speed (baseline)

    vec2  cellUV   = fragTexCoord * gridSize;
    vec2  cellCoord = floor(cellUV);
    vec2  f         = fract(cellUV);

    // Per-cell deterministic values
    float cellRand   = hash(cellCoord);
    vec2  dropOffset = hash2(cellCoord + 73.1);  // drop position within cell [0,1]

    // Spawn gate: cells only have a drop if rainIntensity is high enough
    float spawnThreshold = mix(0.97, 0.15, rainIntensity);
    if (cellRand > spawnThreshold) {
        // No drop in this cell: render plain glass
        vec4 glassTint = vec4(0.88, 0.93, 0.98, 0.3 * push.alphaMultiplier);
        vec3 sceneColor = texture(sceneSnapshot, fragScreenUV).rgb;
        outColor = vec4(mix(sceneColor * glassTint.rgb, sceneColor, 0.5), glassTint.a);
        return;
    }

    // ----------------------------------------------------------------
    // 3. Drop age → wetness
    //
    // Each cell's drop phases through: form → sit → slide off → reset
    // Cycle length varies per cell (1.5–4 seconds)
    // ----------------------------------------------------------------
    float cycleLen = 2.0 + cellRand * 2.0;
    float phase    = fract(time * growSpeed / cycleLen + cellRand);

    // Wetness: rises quickly in first 20% of cycle, holds, falls off at end
    float wetness = smoothstep(0.0, 0.2, phase) * (1.0 - smoothstep(0.75, 1.0, phase));

    // Wiper clearing: instantly zero wetness when wiper passes
    if (inWiperArc) {
        wetness = 0.0;
    }

    // ----------------------------------------------------------------
    // 4. Drop sliding
    //
    // Drops slide down under gravity + car speed.
    // Wind shifts them sideways.
    // ----------------------------------------------------------------
    float slideScale = gravity * (1.0 + carSpeed / 10000.0);
    float slideY     = phase * slideScale * cycleLen;  // total slide in UV
    float slideX     = (windX / 6000.0) * phase * 0.05;

    // Drop center in cell-local UV [0,1]
    vec2 dropCenter = dropOffset * 0.6 + 0.2;
    dropCenter.y   += slideY;
    dropCenter.x   += slideX;
    // Wrap in Y so the drop re-enters from the top
    dropCenter.y    = fract(dropCenter.y);

    // ----------------------------------------------------------------
    // 5. Drop shape (radial SDF)
    //
    // Elongated vertically as it slides faster (stretching).
    // ----------------------------------------------------------------
    float dropRadius   = 0.12 + cellRand * 0.08;
    float aspectY      = 1.0 + slideScale * 3.0;  // taller when sliding faster

    vec2  toCenter     = f - dropCenter;
    toCenter.y         /= aspectY;
    float dropDist     = length(toCenter);

    // Smooth falloff: 1.0 at center, 0.0 at edge
    float dropAlpha    = smoothstep(dropRadius, dropRadius * 0.1, dropDist);
    // Scale by wetness: drop only visible once it has formed
    dropAlpha         *= wetness;

    // ----------------------------------------------------------------
    // 6. Refraction through the drop
    //
    // The drop acts as a convex lens: its surface normal points radially
    // away from the drop center. We use this to offset the screen UV when
    // sampling the scene snapshot, producing visible refraction distortion.
    // ----------------------------------------------------------------
    vec2 dropNormal    = vec2(0.0);
    if (dropDist > 0.001) {
        // Radial normal from drop center (in f-space)
        dropNormal = normalize(toCenter) * dropAlpha;
    }

    // Refraction strength: stronger at drop center, ~0.025 UV units max
    vec2 refractedUV   = fragScreenUV + dropNormal * wetness * 0.07;
    refractedUV        = clamp(refractedUV, vec2(0.001), vec2(0.999));

    // ----------------------------------------------------------------
    // 7. Sample scene through the distorted UV
    // ----------------------------------------------------------------
    vec3 sceneColor = texture(sceneSnapshot, refractedUV).rgb;

    // ----------------------------------------------------------------
    // 8. Glass material properties
    // ----------------------------------------------------------------
    vec4  baseColorSample = texture(baseColorMap, fragTexCoord);
    vec3  glassTint       = baseColorSample.rgb * vec3(0.88, 0.93, 0.98);
    float glassAlpha      = mix(0.20, 0.50, wetness) * push.alphaMultiplier;

    // ----------------------------------------------------------------
    // 9. Specular highlight on the water drop surface
    //
    // Water has a strong Fresnel response. We add a bright highlight
    // toward the sun direction projected onto the glass surface.
    // ----------------------------------------------------------------
    vec3  sunDir          = normalize(camera.sunDirection.xyz);
    vec3  N               = normalize(fragNormal);
    vec3  V               = normalize(camera.cameraPosition.xyz - fragWorldPos);
    float NdotV           = max(dot(N, V), 0.0);
    // Schlick Fresnel for water (F0 ≈ 0.02)
    float fresnel         = 0.02 + (1.0 - 0.02) * pow(1.0 - NdotV, 5.0);
    float sunSpecular     = max(dot(N, sunDir), 0.0);
    vec3  specularColor   = vec3(1.0, 0.98, 0.96) * fresnel * sunSpecular * dropAlpha * 0.8;

    // ----------------------------------------------------------------
    // 10. Composite output
    //
    // We blend:
    //   - Scene color (refracted through water) for the drop region
    //   - Glass tint for the dry region
    //   - Specular highlight on top of the drop
    // ----------------------------------------------------------------
    // Where drops are present: show refracted scene + water tint
    // Where glass is dry: show lightly tinted scene behind glass
    vec3 wetRegion  = mix(sceneColor * glassTint, sceneColor, 0.35) + specularColor;
    vec3 dryRegion  = sceneColor * glassTint;
    vec3 finalColor = mix(dryRegion, wetRegion, dropAlpha);

    outColor = vec4(finalColor, glassAlpha);
}
