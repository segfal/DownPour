#version 450

// ============================================================================
// Deferred Lighting Fragment Shader
//
// Reads the 3 G-Buffer attachments + depth, reconstructs world position,
// and performs Cook-Torrance PBR BRDF + fog + tonemapping.
//
// This consolidates ALL the duplicated PBR lighting code from world.frag,
// car.frag, and terrain.frag into a single shader.
// ============================================================================

const float PI = 3.14159265359;

// --- Sun & ambient (match forward shaders for visual consistency) ---
const vec3  SUN_COLOR          = vec3(1.0, 0.98, 0.92);
const vec3  AMBIENT_SUNNY      = vec3(0.22, 0.24, 0.30);
const vec3  AMBIENT_RAINY      = vec3(0.25, 0.26, 0.28);
const float RAIN_SUN_DIM       = 0.35;

// --- Fog ---
const float FOG_DENSITY_SUNNY  = 0.0000002;
const float FOG_DENSITY_RAINY  = 0.0000012;
const vec3  FOG_COLOR_SUNNY    = vec3(0.75, 0.85, 1.0);
const vec3  FOG_COLOR_RAINY    = vec3(0.55, 0.58, 0.62);

// ============================================================================
// Descriptor Sets
// ============================================================================

// Camera UBO (set 0, binding 0) — same layout as all other shaders
layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    mat4 invViewProjection;
    vec4 sunDirection;
    vec4 cameraPosition;
    vec4 weatherParams;
} camera;

// G-Buffer textures (set 2) — sampled with NEAREST (exact texel values)
layout(set = 2, binding = 0) uniform sampler2D gAlbedoMetallic;
layout(set = 2, binding = 1) uniform sampler2D gNormalRoughness;
layout(set = 2, binding = 2) uniform sampler2D gEmissiveAO;
layout(set = 2, binding = 3) uniform sampler2D gDepth;

// ============================================================================
// Inputs / Outputs
// ============================================================================

layout(location = 0) in vec2 fragUV;
layout(location = 0) out vec4 outColor;

// ============================================================================
// World Position Reconstruction from Depth
//
// Instead of storing world position in a 4th G-Buffer texture (16 bytes/pixel),
// we reconstruct it from depth + inverse view-projection matrix:
//   1. Read depth from depth texture
//   2. Construct clip-space position: (NDC.xy, depth, 1.0)
//   3. Multiply by invViewProjection
//   4. Perspective divide (w)
// ============================================================================

vec3 reconstructWorldPos(vec2 uv, float depth) {
    // UV to NDC: Vulkan NDC is [-1,1] x [-1,1], depth is [0,1]
    vec2 ndc = uv * 2.0 - 1.0;
    vec4 clipPos = vec4(ndc, depth, 1.0);
    vec4 worldPos = camera.invViewProjection * clipPos;
    return worldPos.xyz / worldPos.w;
}

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
// Main
// ============================================================================

void main() {
    float rainIntensity = camera.weatherParams.x;

    // --------------------------------------------------------------------
    // 1. Read G-Buffer
    // --------------------------------------------------------------------
    vec4 albedoMetallic  = texture(gAlbedoMetallic, fragUV);
    vec4 normalRoughness = texture(gNormalRoughness, fragUV);
    vec4 emissiveAO      = texture(gEmissiveAO, fragUV);
    float depth          = texture(gDepth, fragUV).r;

    vec3  albedo    = albedoMetallic.rgb;
    float metallic  = albedoMetallic.a;
    vec3  N         = normalize(normalRoughness.rgb * 2.0 - 1.0);  // Decode from [0,1] to [-1,1]
    float roughness = normalRoughness.a;
    vec3  emissive  = emissiveAO.rgb;
    float ao        = emissiveAO.a;

    // Skip sky pixels (depth == 1.0 means no geometry was written)
    if (depth >= 1.0) {
        outColor = vec4(0.0, 0.0, 0.0, 0.0);
        return;
    }

    // --------------------------------------------------------------------
    // 2. Reconstruct World Position from Depth
    // --------------------------------------------------------------------
    vec3 worldPos = reconstructWorldPos(fragUV, depth);
    float distToCamera = length(camera.cameraPosition.xyz - worldPos);

    // --------------------------------------------------------------------
    // 3. Cook-Torrance PBR BRDF
    // --------------------------------------------------------------------
    vec3 V = normalize(camera.cameraPosition.xyz - worldPos);
    vec3 L = normalize(camera.sunDirection.xyz);
    vec3 H = normalize(V + L);

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.001);
    float NdotH = max(dot(N, H), 0.0);
    float HdotV = max(dot(H, V), 0.0);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    float D = distributionGGX(NdotH, roughness);
    float G = geometrySmith(NdotV, NdotL, roughness);
    vec3  F = fresnelSchlick(HdotV, F0);

    vec3 specular = (D * G * F) / (4.0 * NdotV * NdotL + 0.0001);

    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
    vec3 diffuse = kD * albedo / PI;

    // Sun radiance — reduced during rain (cloud cover)
    float sunIntensity = max(camera.sunDirection.w, 1.0);
    sunIntensity *= mix(1.0, RAIN_SUN_DIM, rainIntensity);
    vec3 sunRadiance = SUN_COLOR * sunIntensity;

    vec3 Lo = (diffuse + specular) * sunRadiance * NdotL;

    // --------------------------------------------------------------------
    // 4. Ambient Lighting
    // --------------------------------------------------------------------
    vec3 ambientBase = mix(AMBIENT_SUNNY, AMBIENT_RAINY, rainIntensity);
    vec3 ambient = ambientBase * albedo * ao;

    vec3 color = ambient + Lo + emissive;

    // --------------------------------------------------------------------
    // 5. Exponential Fog
    // --------------------------------------------------------------------
    float fogDensity = mix(FOG_DENSITY_SUNNY, FOG_DENSITY_RAINY, rainIntensity);
    float fogFactor = 1.0 - exp(-fogDensity * distToCamera);
    vec3 fogColor = mix(FOG_COLOR_SUNNY, FOG_COLOR_RAINY, rainIntensity);
    color = mix(color, fogColor, fogFactor);

    // --------------------------------------------------------------------
    // 6. Reinhard Tone Mapping
    // --------------------------------------------------------------------
    color = color / (color + vec3(1.0));

    // --------------------------------------------------------------------
    // 7. Output
    // --------------------------------------------------------------------
    outColor = vec4(color, 1.0);
}
