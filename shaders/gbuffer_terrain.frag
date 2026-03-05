#version 450

// ============================================================================
// G-Buffer Terrain Fragment Shader
//
// Writes terrain material properties to 3 MRT attachments.
// NO lighting, NO fog, NO tonemapping — those happen in deferred_lighting.frag.
//
// Keeps: stochastic grass texture, road-edge blending, wet darkening.
// ============================================================================

// --- Road-edge geometry ---
const float ROAD_HALF_WIDTH    = 15500.0;
const float EDGE_NOISE_BIAS    = 8000.0;
const float EDGE_FREQ_1        = 0.00015;
const float EDGE_FREQ_2        = 0.0003;
const float EDGE_FREQ_3        = 0.0006;

// --- Blend zones ---
const float DIRT_ZONE_END      = 3000.0;
const float GRASS_ZONE_START   = 15000.0;
const float SPARSE_ZONE_START  = 1000.0;
const float SPARSE_ZONE_END    = 8000.0;

// --- Dirt ---
const vec3  DIRT_COLOR         = vec3(0.35, 0.25, 0.15);
const float DIRT_WET_DARKEN    = 0.6;

// --- Grass ---
const float GRASS_WET_DARKEN   = 0.65;
const vec3  GRASS_TINT         = vec3(1.0, 1.0, 1.0);
const float TILE_VAR_FREQ      = 0.00002;
const float TILE_VAR_MIN       = 0.85;
const float TILE_VAR_RANGE     = 0.30;
const float PATCH_VAR_FREQ     = 0.00007;
const float PATCH_VAR_MIN      = 0.90;
const float PATCH_VAR_RANGE    = 0.20;
const float SPARSE_NOISE_FREQ  = 0.0005;

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

layout(set = 1, binding = 0) uniform sampler2D grassTexture;

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragWorldPos;

// G-Buffer MRT outputs
layout(location = 0) out vec4 gAlbedoMetallic;
layout(location = 1) out vec4 gNormalRoughness;
layout(location = 2) out vec4 gEmissiveAO;

// ============================================================================
// Noise utilities
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

vec3 textureNoTile(sampler2D samp, vec2 uv) {
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

    vec2 uva = uv * ofa.zw + ofa.xy;  vec2 ddxa = ddx * ofa.zw;  vec2 ddya = ddy * ofa.zw;
    vec2 uvb = uv * ofb.zw + ofb.xy;  vec2 ddxb = ddx * ofb.zw;  vec2 ddyb = ddy * ofb.zw;
    vec2 uvc = uv * ofc.zw + ofc.xy;  vec2 ddxc = ddx * ofc.zw;  vec2 ddyc = ddy * ofc.zw;
    vec2 uvd = uv * ofd.zw + ofd.xy;  vec2 ddxd = ddx * ofd.zw;  vec2 ddyd = ddy * ofd.zw;

    vec2 b = smoothstep(0.25, 0.75, fuv);

    return mix(
        mix(textureGrad(samp, uva, ddxa, ddya).rgb,
            textureGrad(samp, uvb, ddxb, ddyb).rgb, b.x),
        mix(textureGrad(samp, uvc, ddxc, ddyc).rgb,
            textureGrad(samp, uvd, ddxd, ddyd).rgb, b.x),
        b.y
    );
}

// ============================================================================
// Main — Write terrain material to G-Buffer
// ============================================================================

void main() {
    float wetness = camera.weatherParams.y;

    // 1. Road-edge blend zone
    float distFromEdge = abs(fragWorldPos.x) - ROAD_HALF_WIDTH;
    float edgeNoise = noise(fragWorldPos.xz * EDGE_FREQ_1)
                    + 0.5 * noise(fragWorldPos.xz * EDGE_FREQ_2)
                    + 0.25 * noise(fragWorldPos.xz * EDGE_FREQ_3);
    edgeNoise /= 1.75;
    float effectiveDist = distFromEdge + (edgeNoise - 0.5) * EDGE_NOISE_BIAS;

    vec3 dirtColor = DIRT_COLOR * mix(1.0, DIRT_WET_DARKEN, wetness);

    // 2. Stochastic grass texture
    vec3 grassColor = textureNoTile(grassTexture, fragTexCoord);
    grassColor *= GRASS_TINT;
    grassColor *= TILE_VAR_MIN + TILE_VAR_RANGE * noise(fragWorldPos.xz * TILE_VAR_FREQ);
    grassColor *= PATCH_VAR_MIN + PATCH_VAR_RANGE * noise(fragWorldPos.xz * PATCH_VAR_FREQ);
    grassColor *= mix(1.0, GRASS_WET_DARKEN, wetness);

    // 3. Blend zones
    float sparseGrass = smoothstep(0.3, 0.6, noise(fragWorldPos.xz * SPARSE_NOISE_FREQ));
    float transitionBlend = smoothstep(SPARSE_ZONE_START, SPARSE_ZONE_END, effectiveDist);
    vec3 sparseColor = mix(dirtColor, grassColor * 0.7, sparseGrass * transitionBlend);

    vec3 surfaceColor;
    if (effectiveDist < DIRT_ZONE_END) {
        surfaceColor = dirtColor;
    } else if (effectiveDist < GRASS_ZONE_START) {
        float t = (effectiveDist - DIRT_ZONE_END) / (GRASS_ZONE_START - DIRT_ZONE_END);
        surfaceColor = mix(sparseColor, grassColor, smoothstep(0.0, 1.0, t));
    } else {
        surfaceColor = grassColor;
    }

    // 4. Write to G-Buffer
    vec3 N = normalize(fragNormal);

    gAlbedoMetallic  = vec4(surfaceColor, 0.0);                // Terrain is non-metallic
    gNormalRoughness = vec4(N * 0.5 + 0.5, 0.9);              // Grass is rough
    gEmissiveAO      = vec4(0.0, 0.0, 0.0, 1.0);             // No emissive, full AO
}
