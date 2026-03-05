#version 450

// ============================================================================
// G-Buffer Car Fragment Shader
//
// Writes car material properties to 3 MRT attachments.
// NO lighting, NO fog, NO tonemapping — those happen in deferred_lighting.frag.
//
// Keeps: texture sampling, normal mapping, wet surface effects.
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

layout(push_constant) uniform PushConstants {
    mat4  model;
    float alphaMultiplier;
} push;

layout(set = 1, binding = 0) uniform sampler2D baseColorMap;
layout(set = 1, binding = 1) uniform sampler2D normalMap;
layout(set = 1, binding = 2) uniform sampler2D metallicRoughnessMap;

layout(location = 0) in vec3 fragNormal;
layout(location = 1) in vec2 fragTexCoord;
layout(location = 2) in vec3 fragWorldPos;

// G-Buffer MRT outputs
layout(location = 0) out vec4 gAlbedoMetallic;    // rgb = albedo, a = metallic
layout(location = 1) out vec4 gNormalRoughness;    // rgb = normal*0.5+0.5, a = roughness
layout(location = 2) out vec4 gEmissiveAO;         // rgb = emissive, a = AO

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
// Main — Write material properties to G-Buffer
// ============================================================================

void main() {
    float wetness = camera.weatherParams.y;

    // 1. Sample material textures
    vec4 baseColorSample = texture(baseColorMap, fragTexCoord);
    vec3 baseColor = baseColorSample.rgb;

    vec2 mrSample = texture(metallicRoughnessMap, fragTexCoord).gb;
    float roughness = clamp(mrSample.x, 0.04, 1.0);
    float metallic  = mrSample.y;

    // 2. Normal mapping
    vec3 N = normalize(fragNormal);
    vec3 normalSample = texture(normalMap, fragTexCoord).rgb;
    bool isDefaultNormalMap = all(greaterThan(normalSample, vec3(0.95)));
    if (!isDefaultNormalMap) {
        vec3 tangentNormal = normalSample * 2.0 - 1.0;
        mat3 TBN = cotangentFrame(N, fragWorldPos, fragTexCoord);
        N = normalize(TBN * tangentNormal);
    }

    // 3. Wet car surface effects
    baseColor *= mix(1.0, 0.75, wetness);
    roughness = mix(roughness, max(roughness * 0.3, 0.04), wetness);

    // 4. Write to G-Buffer
    gAlbedoMetallic  = vec4(baseColor, metallic);
    gNormalRoughness = vec4(N * 0.5 + 0.5, roughness);
    gEmissiveAO      = vec4(0.0, 0.0, 0.0, 1.0);  // No emissive, full AO
}
