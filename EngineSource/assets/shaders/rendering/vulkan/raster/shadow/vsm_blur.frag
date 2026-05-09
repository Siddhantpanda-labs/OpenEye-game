#version 450

layout(location = 0) in vec2 vUv;
layout(location = 0) out vec2 outColor; // R16G16 moments

layout(set = 0, binding = 0) uniform sampler2DArray tSourceMap;

layout(push_constant) uniform PushConstants {
    vec2 direction;   // (1,0) for horizontal, (0,1) for vertical
    int layerIndex;
} uPush;

void main() {
    // Basic 9-tap Gaussian blur weights
    float weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
    
    vec2 texelSize = 1.0 / vec2(textureSize(tSourceMap, 0).xy);
    vec2 offset = uPush.direction * texelSize;
    
    vec3 coord = vec3(vUv, float(uPush.layerIndex));
    vec2 result = texture(tSourceMap, coord).rg * weights[0];
    
    for(int i = 1; i < 5; ++i) {
        result += texture(tSourceMap, coord + vec3(offset * float(i), 0.0)).rg * weights[i];
        result += texture(tSourceMap, coord - vec3(offset * float(i), 0.0)).rg * weights[i];
    }
    
    outColor = result;
}
