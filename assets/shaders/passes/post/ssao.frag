#version 450 core

in vec2 v_TexCoord;
out float FragColor;

uniform sampler2D g_Position;
uniform sampler2D g_Normal;
uniform sampler2D g_Depth;

#include "../../common/camera.glsl"

uniform float u_Radius = 0.5;
uniform float u_Bias = 0.025;
uniform int u_SampleCount = 16;
uniform vec2 u_Resolution;

// Generate random rotation vector
vec3 RandomVec3(vec2 texCoords) {
    float random = fract(sin(dot(texCoords, vec2(12.9898, 78.233))) * 43758.5453);
    float random2 = fract(sin(dot(texCoords + vec2(1.0, 0.0), vec2(12.9898, 78.233))) * 43758.5453);
    float random3 = fract(sin(dot(texCoords + vec2(0.0, 1.0), vec2(12.9898, 78.233))) * 43758.5453);
    return normalize(vec3(random * 2.0 - 1.0, random2 * 2.0 - 1.0, random3));
}

// Generate sample kernel directions (hemisphere)
const vec3 sampleKernel[16] = vec3[](
    vec3( 0.04977, -0.04471, 0.04996),
    vec3( 0.01457,  0.01653, 0.00224),
    vec3(-0.04065, -0.01937, 0.03193),
    vec3( 0.01378, -0.09158, 0.04092),
    vec3( 0.05599,  0.05979, 0.05766),
    vec3( 0.09227,  0.04428, 0.01545),
    vec3(-0.00204, -0.00433, 0.00217),
    vec3(-0.00033, -0.00068, 0.00032),
    vec3( 0.05004, -0.00265, 0.04987),
    vec3( 0.03813,  0.03142, 0.03286),
    vec3(-0.03188,  0.02051, 0.02251),
    vec3( 0.0557,  -0.03697, 0.04049),
    vec3( 0.05743, -0.0229,  0.05454),
    vec3(-0.03555, -0.00099, 0.00016),
    vec3( 0.01698, -0.00898, 0.00374),
    vec3(-0.02729, -0.06378, 0.03263)
);

void main() {
    vec3 position = texture(g_Position, v_TexCoord).rgb;
    vec3 normal = normalize(texture(g_Normal, v_TexCoord).rgb);
    
    // Skip SSAO calculation for sky/background (where position is invalid)
    if (length(position) < 0.001) {
        FragColor = 1.0; // No occlusion
        return;
    }
    
    // Generate random rotation matrix (TBN)
    vec3 randomVec = RandomVec3(v_TexCoord);
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);
    
    float occlusion = 0.0;
    vec2 texelSize = 1.0 / u_Resolution;
    
    for (int i = 0; i < u_SampleCount && i < 16; ++i) {
        // Get sample position in view space
        vec3 samplePos = position + TBN * (sampleKernel[i] * u_Radius);
        
        // Project sample position to screen space (samplePos is in world space)
        // Convert to view space first, then project
        vec4 samplePosView = u_ViewMatrix * vec4(samplePos, 1.0);
        vec4 offset = u_ProjectionMatrix * samplePosView;
        offset.xy /= offset.w;
        offset.xy = offset.xy * 0.5 + 0.5;
        
        // Sample depth at offset position
        vec3 samplePosition = texture(g_Position, offset.xy).rgb;
        
        // Range check and accumulate occlusion (use distance in world space)
        float distanceToSample = length(samplePos - position);
        float rangeCheck = smoothstep(0.0, 1.0, u_Radius / distanceToSample);
        float depthDiff = samplePosView.z - (u_ViewMatrix * vec4(samplePosition, 1.0)).z;
        if (depthDiff > -u_Bias && depthDiff < u_Radius) {
            occlusion += rangeCheck;
        }
    }
    
    occlusion = 1.0 - (occlusion / float(u_SampleCount));
    FragColor = occlusion;
}

