#version 450

// ============================================================================
// Terrain Fragment Shader — Grass strips flanking the road
//
// Stochastic texturing (iq technique) + multi-layer noise for natural edges.
// Reference: Inigo Quilez, "Texture Repetition"
//   https://iquilezles.org/articles/texturerepetition/
// ============================================================================

// ============================================================================
// CONFIG — Tweak these to adjust terrain appearance without touching logic
// Recompile with: glslc shaders/terrain.frag -o shaders/terrain.frag.spv
// ============================================================================

// --- Road-edge geometry (world units, 1000 = 1 meter) ---
const float ROAD_HALF_WIDTH    = 15500.0;  // half the road width in world units
const float EDGE_NOISE_BIAS    = 8000.0;   // ±4m edge irregularity amplitude

// --- Edge noise frequencies (lower = larger features) ---
const float EDGE_FREQ_1        = 0.00015;  // ~6.7km wavelength
const float EDGE_FREQ_2        = 0.0003;   // ~3.3km wavelength
const float EDGE_FREQ_3        = 0.0006;   // ~1.7km wavelength

// --- Blend zones (world units from road edge) ---
const float DIRT_ZONE_END      = 3000.0;   // pure dirt up to 3m from edge
const float GRASS_ZONE_START   = 15000.0;  // full grass beyond 15m
const float SPARSE_ZONE_START  = 1000.0;   // sparse patches begin at 1m
const float SPARSE_ZONE_END    = 8000.0;   // sparse patches end at 8m

// --- Dirt shoulder ---
const vec3  DIRT_COLOR         = vec3(0.35, 0.25, 0.15);
const float DIRT_WET_DARKEN    = 0.6;      // multiplier when fully wet

// --- Grass color ---
const float GRASS_WET_DARKEN   = 0.65;     // multiplier when fully wet
const vec3  GRASS_TINT         = vec3(1.0, 1.0, 1.0); // seasonal tint (e.g. vec3(1.1, 1.0, 0.8) for autumn)

// --- Grass variation noise ---
const float TILE_VAR_FREQ      = 0.00002;  // landscape-scale variation (~50m)
const float TILE_VAR_MIN       = 0.85;     // darkest tiles
const float TILE_VAR_RANGE     = 0.30;     // brightness range
const float PATCH_VAR_FREQ     = 0.00007;  // patch-level variation (~14m)
const float PATCH_VAR_MIN      = 0.90;
const float PATCH_VAR_RANGE    = 0.20;
const float SPARSE_NOISE_FREQ  = 0.0005;   // sparse grass patch frequency

// --- Sun & ambient (should match world.frag for consistency) ---
const vec3  SUN_COLOR          = vec3(1.0, 0.98, 0.92);
const vec3  AMBIENT_SUNNY      = vec3(0.22, 0.24, 0.30);
const vec3  AMBIENT_RAINY      = vec3(0.25, 0.26, 0.28);
const float RAIN_SUN_DIM       = 0.35;

// --- Fog (should match world.frag) ---
const float FOG_DENSITY_SUNNY  = 0.0000002;
const float FOG_DENSITY_RAINY  = 0.0000012;
const vec3  FOG_COLOR_SUNNY    = vec3(0.75, 0.85, 1.0);
const vec3  FOG_COLOR_RAINY    = vec3(0.55, 0.58, 0.62);

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

layout(set = 1, binding = 0) uniform sampler2D grassTexture;

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragWorldPos;
layout(location = 3) in float fragDistance;

layout(location = 0) out vec4 outColor;

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
// Stochastic Texture Sampling (iq Technique 1)
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
// Main
// ============================================================================
void main() {
    float rainIntensity = camera.weatherParams.x;
    float wetness       = camera.weatherParams.y;

    // --------------------------------------------------------------------
    // 1. Road-edge blend zone with noise-perturbed boundary
    // --------------------------------------------------------------------
    float distFromEdge = abs(fragWorldPos.x) - ROAD_HALF_WIDTH;

    // Multi-octave noise for natural edge irregularity
    float edgeNoise = noise(fragWorldPos.xz * EDGE_FREQ_1)
                    + 0.5 * noise(fragWorldPos.xz * EDGE_FREQ_2)
                    + 0.25 * noise(fragWorldPos.xz * EDGE_FREQ_3);
    edgeNoise /= 1.75;
    float noiseBias     = (edgeNoise - 0.5) * EDGE_NOISE_BIAS;
    float effectiveDist = distFromEdge + noiseBias;

    // Dirt / gravel shoulder
    vec3 dirtColor = DIRT_COLOR * mix(1.0, DIRT_WET_DARKEN, wetness);

    // --------------------------------------------------------------------
    // 2. Stochastic grass texture sampling
    // --------------------------------------------------------------------
    vec3 grassColor = textureNoTile(grassTexture, fragTexCoord);

    // Apply seasonal tint
    grassColor *= GRASS_TINT;

    // Landscape-scale variation (~50m wavelength)
    float tileVariation = TILE_VAR_MIN + TILE_VAR_RANGE * noise(fragWorldPos.xz * TILE_VAR_FREQ);
    grassColor *= tileVariation;

    // Patch-level variation (~14m wavelength)
    float patchVariation = PATCH_VAR_MIN + PATCH_VAR_RANGE * noise(fragWorldPos.xz * PATCH_VAR_FREQ);
    grassColor *= patchVariation;

    // Wet grass: darker
    grassColor *= mix(1.0, GRASS_WET_DARKEN, wetness);

    // --------------------------------------------------------------------
    // 3. Blend zones: dirt → sparse → full grass
    // --------------------------------------------------------------------
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

    // --------------------------------------------------------------------
    // 4. Lambertian sun lighting
    // --------------------------------------------------------------------
    vec3 N   = normalize(fragNormal);
    vec3 sunDir = normalize(camera.sunDirection.xyz);

    float NdotL = max(dot(N, sunDir), 0.0);
    float sunIntensity = max(camera.sunDirection.w, 1.0);
    sunIntensity *= mix(1.0, RAIN_SUN_DIM, rainIntensity);

    vec3 ambientBase = mix(AMBIENT_SUNNY, AMBIENT_RAINY, rainIntensity);
    vec3 color = surfaceColor * (ambientBase + SUN_COLOR * NdotL * sunIntensity);

    // --------------------------------------------------------------------
    // 5. Exponential fog
    // --------------------------------------------------------------------
    float fogDensity = mix(FOG_DENSITY_SUNNY, FOG_DENSITY_RAINY, rainIntensity);
    vec3  fogColor   = mix(FOG_COLOR_SUNNY, FOG_COLOR_RAINY, rainIntensity);
    float fogFactor  = 1.0 - exp(-fogDensity * fragDistance);
    color = mix(color, fogColor, fogFactor);

    // --------------------------------------------------------------------
    // 6. Reinhard tone mapping
    // --------------------------------------------------------------------
    color = color / (color + vec3(1.0));

    outColor = vec4(color, 1.0);
}
