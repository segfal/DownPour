#version 450

layout(set = 0, binding = 0) uniform CameraUBO {
    mat4 view;
    mat4 projection;
    mat4 viewProjection;
    vec4 sunDirection;    // xyz = dir, w = intensity
    vec4 cameraPosition;  // xyz = pos, w = time
    vec4 weatherParams;   // x = rainIntensity, y = wetness, z = windX, w = windZ
} camera;

layout(location = 0) in vec3 fragDirection;
layout(location = 0) out vec4 outColor;

void main() {
    vec3 viewDir = normalize(fragDirection);
    vec3 sunDir = normalize(camera.sunDirection.xyz);
    float rainIntensity = camera.weatherParams.x;

    float elevation = viewDir.y;

    // Sky colors: lerp toward overcast gray during rain
    vec3 sunnyZenith  = vec3(0.18, 0.55, 1.6);
    vec3 sunnyHorizon = vec3(0.75, 0.85, 1.0);
    vec3 rainyZenith  = vec3(0.35, 0.38, 0.42);
    vec3 rainyHorizon = vec3(0.45, 0.48, 0.52);

    vec3 zenithColor  = mix(sunnyZenith, rainyZenith, rainIntensity);
    vec3 horizonColor = mix(sunnyHorizon, rainyHorizon, rainIntensity);

    // Gradient based on elevation
    float t = clamp((elevation + 0.1) / 1.1, 0.0, 1.0);
    vec3 skyColor = mix(horizonColor, zenithColor, t);

    // Rayleigh scattering glow near horizon (fades in rain)
    float horizonGlow = pow(1.0 - abs(elevation), 3.0);
    skyColor += vec3(0.30, 0.20, 0.10) * horizonGlow * (1.0 - rainIntensity * 0.7);

    // Sun disc and corona (fade significantly in rain)
    float sunDot = dot(viewDir, sunDir);
    float sunVisibility = 1.0 - rainIntensity * 0.85;
    float sunDisc = smoothstep(0.9995, 0.9999, sunDot) * sunVisibility;
    float corona = pow(max(sunDot, 0.0), 32.0) * 0.5 * sunVisibility;

    vec3 sunColor = vec3(1.0, 0.98, 0.92);
    skyColor += sunColor * (sunDisc + corona);

    // Below-horizon ground fade
    if (elevation < 0.0) {
        vec3 groundColor = mix(vec3(0.1, 0.12, 0.15), vec3(0.15, 0.16, 0.17), rainIntensity);
        skyColor = mix(groundColor, skyColor, clamp(elevation + 0.5, 0.0, 1.0));
    }

    // Reinhard tone mapping
    skyColor = skyColor / (skyColor + vec3(1.0));

    outColor = vec4(skyColor, 1.0);
}
