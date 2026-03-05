#version 450

// ============================================================================
// CONFIG — Tweak these to adjust sky appearance without touching logic
// Recompile with: glslc shaders/basic.frag -o shaders/basic.frag.spv
// ============================================================================

// --- Sky gradient ---
const vec3  SUNNY_ZENITH       = vec3(0.10, 0.40, 1.8);   // deep blue overhead
const vec3  SUNNY_HORIZON      = vec3(0.75, 0.85, 1.0);   // pale blue at horizon
const vec3  RAINY_ZENITH       = vec3(0.35, 0.38, 0.42);  // overcast gray
const vec3  RAINY_HORIZON      = vec3(0.45, 0.48, 0.52);
const float GRADIENT_POWER     = 0.6;     // <1 pushes blue lower, >1 concentrates at zenith

// --- Horizon glow (Rayleigh-like scatter near horizon) ---
const vec3  HORIZON_GLOW_COLOR = vec3(0.15, 0.12, 0.06);  // warm tint
const float HORIZON_GLOW_POWER = 5.0;     // falloff exponent (higher = tighter band)
const float HORIZON_GLOW_RAIN_FADE = 0.7; // how much rain fades the glow (0-1)

// --- Sun disc & corona ---
const vec3  SUN_COLOR          = vec3(1.0, 0.98, 0.92);
const float SUN_DISC_INNER     = 0.9995;  // smoothstep inner edge
const float SUN_DISC_OUTER     = 0.9999;  // smoothstep outer edge
const float SUN_RAIN_FADE      = 0.85;    // how much rain hides the sun (0-1)
const float CORONA_POWER       = 32.0;    // corona falloff (higher = tighter)
const float CORONA_INTENSITY   = 0.5;

// --- Cloud wisps (procedural) ---
const float CLOUD_ENABLE       = 1.0;     // 0.0 = off, 1.0 = on
const float CLOUD_ALTITUDE     = 0.15;    // elevation center of cloud band (0-1)
const float CLOUD_BAND_WIDTH   = 0.25;    // vertical spread of cloud band
const float CLOUD_SCALE        = 3.0;     // noise frequency (higher = smaller wisps)
const float CLOUD_DETAIL_SCALE = 8.0;     // detail noise frequency
const float CLOUD_BRIGHTNESS   = 0.15;    // max brightness added by clouds
const float CLOUD_SPEED        = 0.02;    // drift speed (units/sec)

// --- Below-horizon ground ---
const vec3  GROUND_SUNNY       = vec3(0.10, 0.12, 0.15);
const vec3  GROUND_RAINY       = vec3(0.15, 0.16, 0.17);
const float GROUND_BLEND_RANGE = 0.5;     // how far below horizon to blend

// ============================================================================
// Descriptor Sets
// ============================================================================

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    mat4 invViewProjection;  // inverse(viewProjection) for depth-to-world reconstruction
    vec4 sunDirection;
    vec4 cameraPosition;
    vec4 weatherParams;
} camera;

layout(location = 0) in vec3 fragDirection;
layout(location = 0) out vec4 outColor;

// ============================================================================
// Noise for cloud wisps
// ============================================================================
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);
    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));
    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

float fbm(vec2 p) {
    float v = 0.0;
    float a = 0.5;
    vec2 shift = vec2(100.0);
    for (int i = 0; i < 4; i++) {
        v += a * noise(p);
        p = p * 2.0 + shift;
        a *= 0.5;
    }
    return v;
}

// ============================================================================
// Main
// ============================================================================
void main() {
    vec3 viewDir = normalize(fragDirection);
    vec3 sunDir = normalize(camera.sunDirection.xyz);
    float rainIntensity = camera.weatherParams.x;
    float elapsedTime = camera.cameraPosition.w;

    float elevation = viewDir.y;

    // --- Sky gradient ---
    vec3 zenithColor  = mix(SUNNY_ZENITH, RAINY_ZENITH, rainIntensity);
    vec3 horizonColor = mix(SUNNY_HORIZON, RAINY_HORIZON, rainIntensity);

    float t = pow(clamp(elevation / 1.0, 0.0, 1.0), GRADIENT_POWER);
    vec3 skyColor = mix(horizonColor, zenithColor, t);

    // --- Horizon glow (Rayleigh scatter) ---
    float horizonGlow = pow(max(1.0 - abs(elevation), 0.0), HORIZON_GLOW_POWER);
    skyColor += HORIZON_GLOW_COLOR * horizonGlow * (1.0 - rainIntensity * HORIZON_GLOW_RAIN_FADE);

    // --- Cloud wisps ---
    if (CLOUD_ENABLE > 0.5 && elevation > 0.0) {
        // Project view direction onto a flat plane at cloud altitude
        vec2 cloudUV = viewDir.xz / (elevation + 0.1) * CLOUD_SCALE;
        cloudUV += vec2(elapsedTime * CLOUD_SPEED, elapsedTime * CLOUD_SPEED * 0.3);

        float cloudNoise = fbm(cloudUV);
        float cloudDetail = noise(cloudUV * CLOUD_DETAIL_SCALE / CLOUD_SCALE);
        cloudNoise = cloudNoise * 0.7 + cloudDetail * 0.3;

        // Altitude mask: clouds concentrated around CLOUD_ALTITUDE
        float altMask = exp(-pow((elevation - CLOUD_ALTITUDE) / CLOUD_BAND_WIDTH, 2.0));

        // Threshold to create wispy shapes (not uniform haze)
        float wisps = smoothstep(0.4, 0.7, cloudNoise) * altMask;

        // Clouds brighten the sky, fade in rain (overcast replaces wispy clouds)
        float cloudFade = 1.0 - rainIntensity * 0.9;
        skyColor += vec3(CLOUD_BRIGHTNESS) * wisps * cloudFade;
    }

    // --- Sun disc and corona ---
    float sunDot = dot(viewDir, sunDir);
    float sunVisibility = 1.0 - rainIntensity * SUN_RAIN_FADE;
    float sunDisc = smoothstep(SUN_DISC_INNER, SUN_DISC_OUTER, sunDot) * sunVisibility;
    float corona = pow(max(sunDot, 0.0), CORONA_POWER) * CORONA_INTENSITY * sunVisibility;

    skyColor += SUN_COLOR * (sunDisc + corona);

    // --- Below-horizon ground fade ---
    if (elevation < 0.0) {
        vec3 groundColor = mix(GROUND_SUNNY, GROUND_RAINY, rainIntensity);
        skyColor = mix(groundColor, skyColor, clamp(elevation + GROUND_BLEND_RANGE, 0.0, 1.0));
    }

    // --- Reinhard tone mapping ---
    skyColor = skyColor / (skyColor + vec3(1.0));

    outColor = vec4(skyColor, 1.0);
}
