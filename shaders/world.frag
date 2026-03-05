#version 450

const float PI = 3.14159265359;

// ============================================================================
// CONFIG — Tweak these to adjust road appearance without touching logic
// Recompile with: glslc shaders/world.frag -o shaders/world.frag.spv
// ============================================================================

// --- Texture tiling ---
const float ASPHALT_TILE_SIZE  = 5000.0;  // world units per UV tile (5m at WORLD_SCALE=1000)

// --- Road markings (world units, WORLD_SCALE=1000 → 1000 units = 1 meter) ---
const float CENTER_LINE_WIDTH  = 120.0;   // center dashed yellow line full width
const float CENTER_DASH_PERIOD = 8000.0;  // dash+gap cycle length (8m)
const float CENTER_DASH_LEN    = 3000.0;  // dash portion of the cycle (3m)
const vec3  CENTER_LINE_COLOR  = vec3(0.95, 0.8, 0.1);

const float LANE_LINE_WIDTH    = 100.0;   // lane edge dashed white line full width
const float LANE_LINE_OFFSET   = 3700.0;  // distance from center to lane lines (3.7m)
const vec3  LANE_LINE_COLOR    = vec3(0.9, 0.9, 0.85);

const float SHOULDER_LINE_WIDTH  = 150.0;   // shoulder solid white line full width
const float SHOULDER_LINE_OFFSET = 7400.0;  // distance from center to shoulder (7.4m)

// --- Marking surface ---
const float MARKING_ROUGHNESS  = 0.35;

// --- Wet surface ---
const float WET_DARKEN         = 0.55;    // base color multiplier when fully wet
const float WET_ROUGHNESS      = 0.08;    // roughness when fully wet
const float SPLASH_BUMP_SCALE  = 0.15;    // normal perturbation from splash ripples

// --- Micro-detail noise (breaks up asphalt uniformity) ---
const float DETAIL_NOISE_SCALE = 0.003;   // frequency (higher = finer grain)
const float DETAIL_NOISE_AMP   = 0.08;    // strength (0 = off, 0.1 = subtle)
const float PATCH_NOISE_SCALE  = 0.0003;  // low-freq patchy color variation
const float PATCH_NOISE_AMP    = 0.06;    // strength of patchy variation

// --- Ambient & Sun ---
const vec3  SUN_COLOR          = vec3(1.0, 0.98, 0.92);
const vec3  AMBIENT_SUNNY      = vec3(0.22, 0.24, 0.30);
const vec3  AMBIENT_RAINY      = vec3(0.25, 0.26, 0.28);
const float RAIN_SUN_DIM       = 0.35;    // sun intensity multiplier during full rain

// --- Fog (density values scaled for WORLD_SCALE=1000) ---
const float FOG_DENSITY_SUNNY  = 0.0000002;
const float FOG_DENSITY_RAINY  = 0.0000012;
const vec3  FOG_COLOR_SUNNY    = vec3(0.75, 0.85, 1.0);
const vec3  FOG_COLOR_RAINY    = vec3(0.55, 0.58, 0.62);

// ============================================================================
// Descriptor Sets
// ============================================================================

// Camera UBO (set 0)
layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    mat4 invViewProjection;  // inverse(viewProjection) for depth-to-world reconstruction
    vec4 sunDirection;    // xyz = direction toward sun, w = intensity
    vec4 cameraPosition;  // xyz = world-space position, w = elapsed time
    vec4 weatherParams;   // x = rainIntensity, y = wetness, z = windX, w = windZ
} camera;

// Material textures (set 1) -- glTF PBR metallic-roughness workflow
layout(set = 1, binding = 0) uniform sampler2D baseColorMap;
layout(set = 1, binding = 1) uniform sampler2D normalMap;
layout(set = 1, binding = 2) uniform sampler2D metallicRoughnessMap;

// ============================================================================
// Vertex Shader Inputs
// ============================================================================

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragWorldPos;
layout(location = 3) in float fragDistance;

layout(location = 0) out vec4 outColor;

// ============================================================================
// PBR: GGX/Trowbridge-Reitz Normal Distribution Function
// ============================================================================
float distributionGGX(float NdotH, float roughness) {
    float a  = roughness * roughness;
    float a2 = a * a;
    float denom = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * denom * denom);
}

// ============================================================================
// PBR: Schlick-GGX Geometry Sub-function
// ============================================================================
float geometrySchlickGGX(float NdotV, float k) {
    return NdotV / (NdotV * (1.0 - k) + k);
}

// ============================================================================
// PBR: Smith Geometry Function
// ============================================================================
float geometrySmith(float NdotV, float NdotL, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return geometrySchlickGGX(NdotV, k) * geometrySchlickGGX(NdotL, k);
}

// ============================================================================
// PBR: Schlick Fresnel Approximation
// ============================================================================
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// ============================================================================
// Normal Mapping: Cotangent-Frame TBN
// ============================================================================
mat3 cotangentFrame(vec3 N, vec3 p, vec2 uv) {
    vec3 dp1  = dFdx(p);
    vec3 dp2  = dFdy(p);
    vec2 duv1 = dFdx(uv);
    vec2 duv2 = dFdy(uv);

    vec3 dp2perp = cross(dp2, N);
    vec3 dp1perp = cross(N, dp1);

    vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;

    float invmax = inversesqrt(max(dot(T, T), dot(B, B)));
    return mat3(T * invmax, B * invmax, N);
}

// ============================================================================
// Noise utilities
// ============================================================================
float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

// 4-component hash for stochastic texturing (offset.xy, mirror.zw)
vec4 hash4(vec2 p) {
    return vec4(
        fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453),
        fract(sin(dot(p, vec2(269.5, 183.3))) * 43758.5453),
        fract(sin(dot(p, vec2(420.3,  37.1))) * 43758.5453),
        fract(sin(dot(p, vec2( 53.7, 494.3))) * 43758.5453)
    );
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

// ============================================================================
// Stochastic Texture Sampling — iq Technique 1 (Tile-Based Hash Offsets)
//
// Samples the texture 4 times from neighboring tiles with random UV offsets
// and mirroring, then blends smoothly near tile boundaries. Eliminates
// visible tiling on large surfaces like asphalt.
// ============================================================================
struct StochasticUV {
    vec2 uva, uvb, uvc, uvd;
    vec2 ddxa, ddya, ddxb, ddyb, ddxc, ddyc, ddxd, ddyd;
    vec2 blend;
};

StochasticUV computeStochasticUV(vec2 uv) {
    StochasticUV s;
    vec2 iuv = floor(uv);
    vec2 fuv = fract(uv);

    vec4 ofa = hash4(iuv + vec2(0.0, 0.0));
    vec4 ofb = hash4(iuv + vec2(1.0, 0.0));
    vec4 ofc = hash4(iuv + vec2(0.0, 1.0));
    vec4 ofd = hash4(iuv + vec2(1.0, 1.0));

    vec2 ddx = dFdx(uv);
    vec2 ddy = dFdy(uv);

    ofa.zw = sign(ofa.zw - 0.5);
    ofb.zw = sign(ofb.zw - 0.5);
    ofc.zw = sign(ofc.zw - 0.5);
    ofd.zw = sign(ofd.zw - 0.5);

    s.uva = uv * ofa.zw + ofa.xy;  s.ddxa = ddx * ofa.zw;  s.ddya = ddy * ofa.zw;
    s.uvb = uv * ofb.zw + ofb.xy;  s.ddxb = ddx * ofb.zw;  s.ddyb = ddy * ofb.zw;
    s.uvc = uv * ofc.zw + ofc.xy;  s.ddxc = ddx * ofc.zw;  s.ddyc = ddy * ofc.zw;
    s.uvd = uv * ofd.zw + ofd.xy;  s.ddxd = ddx * ofd.zw;  s.ddyd = ddy * ofd.zw;

    s.blend = smoothstep(0.25, 0.75, fuv);
    return s;
}

vec3 textureNoTile3(sampler2D samp, StochasticUV s) {
    return mix(
        mix(textureGrad(samp, s.uva, s.ddxa, s.ddya).rgb,
            textureGrad(samp, s.uvb, s.ddxb, s.ddyb).rgb, s.blend.x),
        mix(textureGrad(samp, s.uvc, s.ddxc, s.ddyc).rgb,
            textureGrad(samp, s.uvd, s.ddxd, s.ddyd).rgb, s.blend.x),
        s.blend.y
    );
}

vec2 textureNoTile2(sampler2D samp, StochasticUV s) {
    return mix(
        mix(textureGrad(samp, s.uva, s.ddxa, s.ddya).gb,
            textureGrad(samp, s.uvb, s.ddxb, s.ddyb).gb, s.blend.x),
        mix(textureGrad(samp, s.uvc, s.ddxc, s.ddyc).gb,
            textureGrad(samp, s.uvd, s.ddxd, s.ddyd).gb, s.blend.x),
        s.blend.y
    );
}

// ============================================================================
// Procedural splash ripples — animated expanding rings on wet road
// ============================================================================
float splashRipples(vec2 worldXZ, float time, float intensity) {
    if (intensity < 0.01) return 0.0;

    float total = 0.0;

    for (int layer = 0; layer < 3; layer++) {
        float scale = 1.5 + float(layer) * 0.7;
        vec2 cellCoord = worldXZ * scale;
        vec2 cell = floor(cellCoord);
        vec2 f = fract(cellCoord);

        // Random splash center within cell
        vec2 center = vec2(hash(cell + float(layer) * 17.3),
                           hash(cell + vec2(7.0, 13.0) + float(layer) * 5.1));

        // Cycle: new splash every 0.6-1.0 seconds per cell
        float h = hash(cell + float(layer) * 31.0);
        float cycle = 0.6 + h * 0.4;
        float phase = mod(time + h * cycle, cycle);
        float age = phase / cycle;

        // Expanding ring
        float dist = length(f - center);
        float ringRadius = age * 0.4;
        float ringWidth = 0.04;
        float ring = smoothstep(ringRadius - ringWidth, ringRadius, dist)
                   * smoothstep(ringRadius + ringWidth, ringRadius, dist);

        // Fade with age²
        float fade = (1.0 - age) * (1.0 - age);

        total += ring * fade;
    }

    return clamp(total * intensity, 0.0, 1.0);
}

// ============================================================================
// Procedural Road Markings
// ============================================================================
void roadMarkings(vec3 worldPos, out vec3 markingColor, out float markingBlend) {
    markingColor = vec3(0.0);
    markingBlend = 0.0;

    float x = worldPos.x;
    float z = worldPos.z;

    float dxdScreen = fwidth(x);
    float dzdScreen = fwidth(z);

    // Center dashed yellow line
    float centerHW = CENTER_LINE_WIDTH * 0.5;
    float centerEdge = smoothstep(centerHW + dxdScreen, centerHW - dxdScreen, abs(x));
    float dashPhase = mod(z, CENTER_DASH_PERIOD);
    float dashMask = smoothstep(CENTER_DASH_LEN + dzdScreen, CENTER_DASH_LEN - dzdScreen, dashPhase)
                   * smoothstep(-dzdScreen, dzdScreen, dashPhase);
    float centerMark = centerEdge * dashMask;

    // Lane edge dashed white lines
    float laneHW = LANE_LINE_WIDTH * 0.5;
    float laneEdge = smoothstep(laneHW + dxdScreen, laneHW - dxdScreen, abs(abs(x) - LANE_LINE_OFFSET));
    float laneMark = laneEdge * dashMask;

    // Shoulder solid white lines
    float shoulderHW = SHOULDER_LINE_WIDTH * 0.5;
    float shoulderMark = smoothstep(shoulderHW + dxdScreen, shoulderHW - dxdScreen, abs(abs(x) - SHOULDER_LINE_OFFSET));

    markingBlend = max(shoulderMark, laneMark);
    markingColor = LANE_LINE_COLOR;

    if (centerMark > 0.001) {
        markingBlend = centerMark;
        markingColor = CENTER_LINE_COLOR;
    }
}

// ============================================================================
// Main Fragment Shader
// ============================================================================
void main() {
    float rainIntensity = camera.weatherParams.x;
    float wetness       = camera.weatherParams.y;
    float elapsedTime   = camera.cameraPosition.w;

    // --------------------------------------------------------------------
    // 1. Sample Material Textures (stochastic anti-tiling)
    // --------------------------------------------------------------------
    // World-space UVs for proper tiling regardless of model scale
    vec2 worldUV = fragWorldPos.xz / ASPHALT_TILE_SIZE;
    StochasticUV suv = computeStochasticUV(worldUV);

    vec3 baseColor = textureNoTile3(baseColorMap, suv);

    // Micro-detail noise: fine grain breaks up texture uniformity
    float detailNoise = noise(fragWorldPos.xz * DETAIL_NOISE_SCALE);
    baseColor *= 1.0 + (detailNoise - 0.5) * DETAIL_NOISE_AMP * 2.0;

    // Patchy low-freq variation: simulates wear, oil stains, aging
    float patchNoise = noise(fragWorldPos.xz * PATCH_NOISE_SCALE);
    baseColor *= 1.0 + (patchNoise - 0.5) * PATCH_NOISE_AMP * 2.0;

    vec2 mrSample = textureNoTile2(metallicRoughnessMap, suv);
    float roughness = clamp(mrSample.x, 0.04, 1.0);
    float metallic  = mrSample.y;

    // Roughness also varies slightly with patch noise (worn areas smoother)
    roughness *= 1.0 + (patchNoise - 0.5) * 0.1;

    // --------------------------------------------------------------------
    // 2. Normal Mapping
    // --------------------------------------------------------------------
    vec3 N = normalize(fragNormal);

    vec3 normalSample = textureNoTile3(normalMap, suv);
    bool isDefaultNormalMap = all(greaterThan(normalSample, vec3(0.95)));

    if (!isDefaultNormalMap) {
        vec3 tangentNormal = normalSample * 2.0 - 1.0;
        mat3 TBN = cotangentFrame(N, fragWorldPos, worldUV);
        N = normalize(TBN * tangentNormal);
    }

    // --------------------------------------------------------------------
    // 3. Procedural Road Markings
    // --------------------------------------------------------------------
    vec3 markingColor;
    float markingBlend;
    roadMarkings(fragWorldPos, markingColor, markingBlend);

    baseColor = mix(baseColor, markingColor, markingBlend);
    roughness = mix(roughness, MARKING_ROUGHNESS, markingBlend);
    metallic  = mix(metallic, 0.0, markingBlend);

    // --------------------------------------------------------------------
    // 3b. Wet Surface Effects
    // --------------------------------------------------------------------
    // Wet asphalt: darker (water fills micro-cavities) and smoother (water film)
    baseColor *= mix(1.0, WET_DARKEN, wetness);
    roughness  = mix(roughness, WET_ROUGHNESS, wetness);

    // Splash ripple normal perturbation on wet road
    float splash = splashRipples(fragWorldPos.xz, elapsedTime, rainIntensity);
    if (splash > 0.01) {
        float splashBump = splash * SPLASH_BUMP_SCALE;
        N = normalize(N + vec3(splashBump, 0.0, splashBump));
    }

    // --------------------------------------------------------------------
    // 4. Cook-Torrance PBR BRDF
    // --------------------------------------------------------------------
    vec3 V = normalize(camera.cameraPosition.xyz - fragWorldPos);
    vec3 L = normalize(camera.sunDirection.xyz);
    vec3 H = normalize(V + L);

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.001);
    float NdotH = max(dot(N, H), 0.0);
    float HdotV = max(dot(H, V), 0.0);

    vec3 F0 = mix(vec3(0.04), baseColor, metallic);

    float D = distributionGGX(NdotH, roughness);
    float G = geometrySmith(NdotV, NdotL, roughness);
    vec3  F = fresnelSchlick(HdotV, F0);

    vec3 specular = (D * G * F) / (4.0 * NdotV * NdotL + 0.0001);

    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
    vec3 diffuse = kD * baseColor / PI;

    // Sun radiance — reduced during rain (cloud cover)
    float sunIntensity = max(camera.sunDirection.w, 1.0);
    sunIntensity *= mix(1.0, RAIN_SUN_DIM, rainIntensity);
    vec3 sunRadiance = SUN_COLOR * sunIntensity;

    vec3 Lo = (diffuse + specular) * sunRadiance * NdotL;

    // --------------------------------------------------------------------
    // 5. Ambient Lighting (increases slightly in rain — overcast scatter)
    // --------------------------------------------------------------------
    vec3 ambientBase = mix(AMBIENT_SUNNY, AMBIENT_RAINY, rainIntensity);
    vec3 ambient = ambientBase * baseColor;

    vec3 color = ambient + Lo;

    // --------------------------------------------------------------------
    // 6. Exponential Fog (density increases with rain)
    // --------------------------------------------------------------------
    float fogDensity = mix(FOG_DENSITY_SUNNY, FOG_DENSITY_RAINY, rainIntensity);
    float fogFactor = 1.0 - exp(-fogDensity * fragDistance);
    vec3 fogColor = mix(FOG_COLOR_SUNNY, FOG_COLOR_RAINY, rainIntensity);
    color = mix(color, fogColor, fogFactor);

    // --------------------------------------------------------------------
    // 7. Reinhard Tone Mapping
    // --------------------------------------------------------------------
    color = color / (color + vec3(1.0));

    // --------------------------------------------------------------------
    // 8. Output
    // --------------------------------------------------------------------
    outColor = vec4(color, 1.0);
}
