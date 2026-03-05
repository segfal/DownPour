#version 450

const float PI = 3.14159265359;

// ============================================================================
// Descriptor Sets (matches world.vert UBO layout)
// ============================================================================

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    mat4 invViewProjection;  // inverse(viewProjection) for depth-to-world reconstruction
    vec4 sunDirection;    // xyz = direction toward sun, w = intensity
    vec4 cameraPosition;  // xyz = world-space position, w = elapsed time
    vec4 weatherParams;   // x = rainIntensity, y = wetness, z = windX, w = windZ
} camera;

// Push constants (matches car.vert layout)
layout(push_constant) uniform PushConstants {
    mat4  model;
    float alphaMultiplier;  // 1.0 for opaque, <1.0 for glass/mirrors
} push;

// Material textures (set 1) -- glTF PBR metallic-roughness workflow
layout(set = 1, binding = 0) uniform sampler2D baseColorMap;
layout(set = 1, binding = 1) uniform sampler2D normalMap;
layout(set = 1, binding = 2) uniform sampler2D metallicRoughnessMap;

// ============================================================================
// Vertex Shader Inputs (matches world.vert outputs)
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
// Main Fragment Shader — Car PBR (no road effects)
// ============================================================================
void main() {
    float rainIntensity = camera.weatherParams.x;
    float wetness       = camera.weatherParams.y;

    // --------------------------------------------------------------------
    // 1. Sample Material Textures (direct sampling, no stochastic tiling)
    // --------------------------------------------------------------------
    vec4 baseColorSample = texture(baseColorMap, fragTexCoord);
    vec3 baseColor = baseColorSample.rgb;
    float alpha = baseColorSample.a;

    vec2 mrSample = texture(metallicRoughnessMap, fragTexCoord).gb;
    float roughness = clamp(mrSample.x, 0.04, 1.0);
    float metallic  = mrSample.y;

    // --------------------------------------------------------------------
    // 2. Normal Mapping
    // --------------------------------------------------------------------
    vec3 N = normalize(fragNormal);

    vec3 normalSample = texture(normalMap, fragTexCoord).rgb;
    bool isDefaultNormalMap = all(greaterThan(normalSample, vec3(0.95)));

    if (!isDefaultNormalMap) {
        vec3 tangentNormal = normalSample * 2.0 - 1.0;
        mat3 TBN = cotangentFrame(N, fragWorldPos, fragTexCoord);
        N = normalize(TBN * tangentNormal);
    }

    // --------------------------------------------------------------------
    // 3. Wet Car Surface Effects
    // A thin water film darkens the albedo and dramatically reduces roughness,
    // creating sharp specular highlights and grazing-angle reflections.
    // --------------------------------------------------------------------
    // Albedo: water absorbs light → surface darkens when wet
    baseColor *= mix(1.0, 0.75, wetness);
    // Roughness: water film makes the surface much shinier
    roughness = mix(roughness, max(roughness * 0.3, 0.04), wetness);

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

    // F0: water film (n≈1.33) raises the minimum reflectance at grazing angles
    vec3 F0 = mix(vec3(0.04), baseColor, metallic);
    // Blend toward water's F0 (≈0.02 at normal incidence, but higher grazing) when wet
    F0 = mix(F0, max(F0, vec3(0.06)), wetness * 0.6);

    float D = distributionGGX(NdotH, roughness);
    float G = geometrySmith(NdotV, NdotL, roughness);
    vec3  F = fresnelSchlick(HdotV, F0);

    vec3 specular = (D * G * F) / (4.0 * NdotV * NdotL + 0.0001);

    vec3 kS = F;
    vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
    vec3 diffuse = kD * baseColor / PI;

    // Sun radiance — reduced during rain (cloud cover)
    vec3 sunColor = vec3(1.0, 0.98, 0.92);
    float sunIntensity = max(camera.sunDirection.w, 1.0);
    sunIntensity *= mix(1.0, 0.35, rainIntensity);
    vec3 sunRadiance = sunColor * sunIntensity;

    vec3 Lo = (diffuse + specular) * sunRadiance * NdotL;

    // --------------------------------------------------------------------
    // 5. Ambient Lighting
    // --------------------------------------------------------------------
    vec3 ambientBase = mix(vec3(0.22, 0.24, 0.30), vec3(0.25, 0.26, 0.28), rainIntensity);
    vec3 ambient = ambientBase * baseColor;

    vec3 color = ambient + Lo;

    // --------------------------------------------------------------------
    // 6. Exponential Fog (same as world shader for consistency)
    // --------------------------------------------------------------------
    float fogDensity = mix(0.0000002, 0.0000012, rainIntensity);
    float fogFactor = 1.0 - exp(-fogDensity * fragDistance);
    vec3 fogColor = mix(vec3(0.75, 0.85, 1.0), vec3(0.55, 0.58, 0.62), rainIntensity);
    color = mix(color, fogColor, fogFactor);

    // --------------------------------------------------------------------
    // 7. Reinhard Tone Mapping
    // --------------------------------------------------------------------
    color = color / (color + vec3(1.0));

    // --------------------------------------------------------------------
    // 8. Output
    // --------------------------------------------------------------------
    outColor = vec4(color, alpha * push.alphaMultiplier);
}
