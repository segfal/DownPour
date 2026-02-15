#version 450

// ============================================================================
// Terrain Fragment Shader — Grass strips flanking a 310m-wide road
//
// Inspired by Slow Roads (slowroads.io) procedural texturing approach:
// - Stochastic texturing to eliminate tiling repetition (iq technique)
// - Multi-layer noise for natural edge irregularity
// - Wet surface effects for rain interaction
//
// Reference: Inigo Quilez, "Texture Repetition"
//   https://iquilezles.org/articles/texturerepetition/
// ============================================================================

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    vec4 sunDirection;    // xyz = direction toward sun, w = intensity
    vec4 cameraPosition;  // xyz = world-space camera pos, w = elapsed time
    vec4 weatherParams;   // x = rainIntensity, y = wetness, z = windX, w = windZ
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
    f = f * f * (3.0 - 2.0 * f);  // Hermite smoothstep

    float a = hash(i);
    float b = hash(i + vec2(1.0, 0.0));
    float c = hash(i + vec2(0.0, 1.0));
    float d = hash(i + vec2(1.0, 1.0));

    return mix(mix(a, b, f.x), mix(c, d, f.x), f.y);
}

// ============================================================================
// Stochastic Texture Sampling — Tile-Based Hash Offsets (iq Technique 1)
//
// Samples the texture 4 times from neighboring tiles, each with a random
// UV offset and random mirroring. Blends smoothly near tile boundaries
// using smoothstep. This eliminates visible tiling repetition.
//
// Cost: 4 texture samples instead of 1, but completely hides the tile grid.
// ============================================================================
vec3 textureNoTile(sampler2D samp, vec2 uv) {
    vec2 iuv = floor(uv);
    vec2 fuv = fract(uv);

    // Per-tile random transform for 4 neighboring tiles
    vec4 ofa = hash4(iuv + vec2(0.0, 0.0));
    vec4 ofb = hash4(iuv + vec2(1.0, 0.0));
    vec4 ofc = hash4(iuv + vec2(0.0, 1.0));
    vec4 ofd = hash4(iuv + vec2(1.0, 1.0));

    // Screen-space derivatives for correct mipmap filtering
    vec2 ddx = dFdx(uv);
    vec2 ddy = dFdy(uv);

    // Convert hash [0,1] to mirror sign {-1, +1}
    ofa.zw = sign(ofa.zw - 0.5);
    ofb.zw = sign(ofb.zw - 0.5);
    ofc.zw = sign(ofc.zw - 0.5);
    ofd.zw = sign(ofd.zw - 0.5);

    // Apply offset + mirror to UVs and their derivatives
    vec2 uva = uv * ofa.zw + ofa.xy;  vec2 ddxa = ddx * ofa.zw;  vec2 ddya = ddy * ofa.zw;
    vec2 uvb = uv * ofb.zw + ofb.xy;  vec2 ddxb = ddx * ofb.zw;  vec2 ddyb = ddy * ofb.zw;
    vec2 uvc = uv * ofc.zw + ofc.xy;  vec2 ddxc = ddx * ofc.zw;  vec2 ddyc = ddy * ofc.zw;
    vec2 uvd = uv * ofd.zw + ofd.xy;  vec2 ddxd = ddx * ofd.zw;  vec2 ddyd = ddy * ofd.zw;

    // Smooth blend near tile boundaries
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
    float roadHalfWidth = 155.0;
    float distFromEdge  = abs(fragWorldPos.x) - roadHalfWidth;

    // Multi-octave noise for more natural edge irregularity
    float edgeNoise = noise(fragWorldPos.xz * 0.15)
                    + 0.5 * noise(fragWorldPos.xz * 0.3)
                    + 0.25 * noise(fragWorldPos.xz * 0.6);
    edgeNoise /= 1.75;  // Normalize to [0, 1]
    float noiseBias     = (edgeNoise - 0.5) * 8.0;  // +/- 4m perturbation
    float effectiveDist = distFromEdge + noiseBias;

    // Dirt / gravel shoulder (warm brown, darkens when wet)
    vec3 dirtColor = vec3(0.35, 0.25, 0.15);
    dirtColor *= mix(1.0, 0.6, wetness);

    // --------------------------------------------------------------------
    // 2. Stochastic grass texture sampling (no visible tiling)
    // --------------------------------------------------------------------
    vec3 grassColor = textureNoTile(grassTexture, fragTexCoord);

    // Low-frequency color variation (~50m wavelength) adds landscape-scale variety
    float tileVariation = 0.85 + 0.3 * noise(fragWorldPos.xz * 0.02);
    grassColor *= tileVariation;

    // Medium-frequency variation (~15m) adds patch-level variety
    float patchVariation = 0.9 + 0.2 * noise(fragWorldPos.xz * 0.07);
    grassColor *= patchVariation;

    // Wet grass: darker and slightly more saturated
    grassColor *= mix(1.0, 0.65, wetness);

    // Blend zones: 0-3m = dirt, 3-15m = transition, 15m+ = full grass
    float grassBlend = smoothstep(3.0, 15.0, effectiveDist);

    // Sparse grass patches in transition zone
    float sparseGrass = smoothstep(0.3, 0.6, noise(fragWorldPos.xz * 0.5));
    float transitionBlend = smoothstep(1.0, 8.0, effectiveDist);
    vec3 sparseColor = mix(dirtColor, grassColor * 0.7, sparseGrass * transitionBlend);

    // Final surface: dirt → sparse → full grass
    vec3 surfaceColor;
    if (effectiveDist < 3.0) {
        surfaceColor = dirtColor;
    } else if (effectiveDist < 15.0) {
        float t = (effectiveDist - 3.0) / 12.0;
        surfaceColor = mix(sparseColor, grassColor, smoothstep(0.0, 1.0, t));
    } else {
        surfaceColor = grassColor;
    }

    // --------------------------------------------------------------------
    // 3. Lambertian sun lighting
    // --------------------------------------------------------------------
    vec3 N        = normalize(fragNormal);
    vec3 sunDir   = normalize(camera.sunDirection.xyz);
    vec3 sunColor = vec3(1.0, 0.98, 0.92);

    float NdotL = max(dot(N, sunDir), 0.0);
    float sunIntensity = max(camera.sunDirection.w, 1.0);
    sunIntensity *= mix(1.0, 0.35, rainIntensity);

    vec3 ambientBase = mix(vec3(0.22, 0.24, 0.30), vec3(0.25, 0.26, 0.28), rainIntensity);

    vec3 color = surfaceColor * (ambientBase + sunColor * NdotL * sunIntensity);

    // --------------------------------------------------------------------
    // 4. Exponential fog (density increases with rain)
    // --------------------------------------------------------------------
    float fogDensity = mix(0.0000002, 0.0000012, rainIntensity); // scaled for 1000x environment
    vec3  fogColor   = mix(vec3(0.75, 0.85, 1.0), vec3(0.55, 0.58, 0.62), rainIntensity);
    float fogFactor  = 1.0 - exp(-fogDensity * fragDistance);
    color = mix(color, fogColor, fogFactor);

    // --------------------------------------------------------------------
    // 5. Reinhard tone mapping
    // --------------------------------------------------------------------
    color = color / (color + vec3(1.0));

    outColor = vec4(color, 1.0);
}
