#version 450

// ============================================================================
// G-Buffer Road Fragment Shader
//
// Writes road material properties to 3 MRT attachments.
// NO lighting, NO fog, NO tonemapping — those happen in deferred_lighting.frag.
//
// Keeps: texture sampling, stochastic anti-tiling, normal mapping,
//        road markings, wet surface darkening, splash normal perturbation.
// ============================================================================

// --- Texture tiling ---
const float ASPHALT_TILE_SIZE  = 5000.0;

// --- Road markings ---
const float CENTER_LINE_WIDTH  = 120.0;
const float CENTER_DASH_PERIOD = 8000.0;
const float CENTER_DASH_LEN    = 3000.0;
const vec3  CENTER_LINE_COLOR  = vec3(0.95, 0.8, 0.1);
const float LANE_LINE_WIDTH    = 100.0;
const float LANE_LINE_OFFSET   = 3700.0;
const vec3  LANE_LINE_COLOR    = vec3(0.9, 0.9, 0.85);
const float SHOULDER_LINE_WIDTH  = 150.0;
const float SHOULDER_LINE_OFFSET = 7400.0;
const float MARKING_ROUGHNESS  = 0.35;

// --- Wet surface ---
const float WET_DARKEN         = 0.55;
const float WET_ROUGHNESS      = 0.08;
const float SPLASH_BUMP_SCALE  = 0.15;

// --- Micro-detail noise ---
const float DETAIL_NOISE_SCALE = 0.003;
const float DETAIL_NOISE_AMP   = 0.08;
const float PATCH_NOISE_SCALE  = 0.0003;
const float PATCH_NOISE_AMP    = 0.06;

// ============================================================================
// Descriptor Sets
// ============================================================================

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    mat4 invViewProjection;
    vec4 sunDirection;
    vec4 cameraPosition;
    vec4 weatherParams;
} camera;

layout(set = 1, binding = 0) uniform sampler2D baseColorMap;
layout(set = 1, binding = 1) uniform sampler2D normalMap;
layout(set = 1, binding = 2) uniform sampler2D metallicRoughnessMap;

// ============================================================================
// Vertex Inputs
// ============================================================================

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragWorldPos;

// ============================================================================
// G-Buffer MRT Outputs
// ============================================================================

layout(location = 0) out vec4 gAlbedoMetallic;    // rgb = albedo, a = metallic
layout(location = 1) out vec4 gNormalRoughness;    // rgb = normal*0.5+0.5, a = roughness
layout(location = 2) out vec4 gEmissiveAO;         // rgb = emissive, a = AO

// ============================================================================
// Noise utilities (same as world.frag)
// ============================================================================

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

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
// Stochastic Texture Sampling
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
// Splash Ripples (same as world.frag)
// ============================================================================

float splashRipples(vec2 worldXZ, float time, float intensity) {
    if (intensity < 0.01) return 0.0;

    float total = 0.0;
    for (int layer = 0; layer < 3; layer++) {
        float scale = 1.5 + float(layer) * 0.7;
        vec2 cellCoord = worldXZ * scale;
        vec2 cell = floor(cellCoord);
        vec2 f = fract(cellCoord);

        vec2 center = vec2(hash(cell + float(layer) * 17.3),
                           hash(cell + vec2(7.0, 13.0) + float(layer) * 5.1));
        float h = hash(cell + float(layer) * 31.0);
        float cycle = 0.6 + h * 0.4;
        float phase = mod(time + h * cycle, cycle);
        float age = phase / cycle;

        float dist = length(f - center);
        float ringRadius = age * 0.4;
        float ringWidth = 0.04;
        float ring = smoothstep(ringRadius - ringWidth, ringRadius, dist)
                   * smoothstep(ringRadius + ringWidth, ringRadius, dist);
        float fade = (1.0 - age) * (1.0 - age);
        total += ring * fade;
    }
    return clamp(total * intensity, 0.0, 1.0);
}

// ============================================================================
// Road Markings
// ============================================================================

void roadMarkings(vec3 worldPos, out vec3 markingColor, out float markingBlend) {
    markingColor = vec3(0.0);
    markingBlend = 0.0;

    float x = worldPos.x;
    float z = worldPos.z;
    float dxdScreen = fwidth(x);
    float dzdScreen = fwidth(z);

    float centerHW = CENTER_LINE_WIDTH * 0.5;
    float centerEdge = smoothstep(centerHW + dxdScreen, centerHW - dxdScreen, abs(x));
    float dashPhase = mod(z, CENTER_DASH_PERIOD);
    float dashMask = smoothstep(CENTER_DASH_LEN + dzdScreen, CENTER_DASH_LEN - dzdScreen, dashPhase)
                   * smoothstep(-dzdScreen, dzdScreen, dashPhase);
    float centerMark = centerEdge * dashMask;

    float laneHW = LANE_LINE_WIDTH * 0.5;
    float laneEdge = smoothstep(laneHW + dxdScreen, laneHW - dxdScreen, abs(abs(x) - LANE_LINE_OFFSET));
    float laneMark = laneEdge * dashMask;

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
// Main — Write material properties to G-Buffer
// ============================================================================

void main() {
    float rainIntensity = camera.weatherParams.x;
    float wetness       = camera.weatherParams.y;
    float elapsedTime   = camera.cameraPosition.w;

    // 1. Sample material textures (stochastic anti-tiling)
    vec2 worldUV = fragWorldPos.xz / ASPHALT_TILE_SIZE;
    StochasticUV suv = computeStochasticUV(worldUV);

    vec3 baseColor = textureNoTile3(baseColorMap, suv);

    float detailNoise = noise(fragWorldPos.xz * DETAIL_NOISE_SCALE);
    baseColor *= 1.0 + (detailNoise - 0.5) * DETAIL_NOISE_AMP * 2.0;

    float patchNoise = noise(fragWorldPos.xz * PATCH_NOISE_SCALE);
    baseColor *= 1.0 + (patchNoise - 0.5) * PATCH_NOISE_AMP * 2.0;

    vec2 mrSample = textureNoTile2(metallicRoughnessMap, suv);
    float roughness = clamp(mrSample.x, 0.04, 1.0);
    float metallic  = mrSample.y;
    roughness *= 1.0 + (patchNoise - 0.5) * 0.1;

    // 2. Normal mapping
    vec3 N = normalize(fragNormal);
    vec3 normalSample = textureNoTile3(normalMap, suv);
    bool isDefaultNormalMap = all(greaterThan(normalSample, vec3(0.95)));
    if (!isDefaultNormalMap) {
        vec3 tangentNormal = normalSample * 2.0 - 1.0;
        mat3 TBN = cotangentFrame(N, fragWorldPos, worldUV);
        N = normalize(TBN * tangentNormal);
    }

    // 3. Road markings
    vec3 markingColor;
    float markingBlend;
    roadMarkings(fragWorldPos, markingColor, markingBlend);
    baseColor = mix(baseColor, markingColor, markingBlend);
    roughness = mix(roughness, MARKING_ROUGHNESS, markingBlend);
    metallic  = mix(metallic, 0.0, markingBlend);

    // 4. Wet surface effects (darkening + roughness reduction)
    baseColor *= mix(1.0, WET_DARKEN, wetness);
    roughness  = mix(roughness, WET_ROUGHNESS, wetness);

    // 5. Splash ripple normal perturbation
    float splash = splashRipples(fragWorldPos.xz, elapsedTime, rainIntensity);
    if (splash > 0.01) {
        float splashBump = splash * SPLASH_BUMP_SCALE;
        N = normalize(N + vec3(splashBump, 0.0, splashBump));
    }

    // 6. Write to G-Buffer
    gAlbedoMetallic  = vec4(baseColor, metallic);
    gNormalRoughness = vec4(N * 0.5 + 0.5, roughness);
    gEmissiveAO      = vec4(0.0, 0.0, 0.0, 1.0);  // Road has no emissive, full AO
}
