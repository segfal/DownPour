#version 450

layout(location = 0) in vec2 fragTexCoord;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragWorldPos;

layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform sampler2D wetnessMap;
layout(set = 1, binding = 1) uniform sampler2D flowMap;
layout(set = 1, binding = 2) uniform sampler2D sceneTexture;

layout(push_constant) uniform PushConstants {
    float wiperAngle;
    float rainIntensity;
    float time;
} pc;

void main() {
    vec2 uv = fragTexCoord;
    
    // Sample wetness (how much water is here)
    float wetness = texture(wetnessMap, uv).r;
    
    // Apply wiper clearing effect
    // Calculate distance from wiper path
    float wiperClearance = 0.0;
    float distFromCenter = abs(uv.x - 0.5);
    float wiperPos = (pc.wiperAngle / 90.0) * 0.5 + 0.5;  // Map -45..45 to 0..1
    if (abs(uv.y - wiperPos) < 0.05) {  // Wiper blade width
        wiperClearance = 1.0 - (distFromCenter * 2.0);
    }
    wetness *= (1.0 - wiperClearance);
    
    // Sample flow direction for rivulets
    vec2 flow = texture(flowMap, uv).rg;
    
    // Refraction based on wetness
    vec2 refractedUV = uv + flow * wetness * 0.05;
    
    // Sample scene with refraction
    vec3 sceneColor = texture(sceneTexture, refractedUV).rgb;
    
    // Add water droplet highlights
    float droplets = fract(sin(dot(uv * 100.0, vec2(12.9898, 78.233))) * 43758.5453);
    if (wetness > 0.5 && droplets > 0.95) {
        sceneColor += vec3(0.3) * wetness;  // Specular highlights
    }
    
    // Output with slight blue tint for water
    outColor = vec4(mix(sceneColor, sceneColor * vec3(0.9, 0.95, 1.0), wetness * 0.3), 1.0);
}

