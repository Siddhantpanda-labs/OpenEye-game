#version 460

layout(location = 0) in vec2 v_TexCoord;
layout(location = 0) out float FragColor;

layout(binding = 0) uniform sampler2D g_Position;
layout(binding = 1) uniform sampler2D g_Normal;
layout(binding = 2) uniform sampler2D g_Depth;

layout(push_constant, std430) uniform SSAOPush {
    mat4 u_ViewMatrix;
    mat4 u_ProjectionMatrix;
    vec2 u_Resolution;
    float u_Radius;
    float u_Bias;
    int u_SampleCount;
} pc;

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

vec3 RandomVec3(vec2 texCoords) {
    float random = fract(sin(dot(texCoords, vec2(12.9898, 78.233))) * 43758.5453);
    float random2 = fract(sin(dot(texCoords + vec2(1.0, 0.0), vec2(12.9898, 78.233))) * 43758.5453);
    float random3 = fract(sin(dot(texCoords + vec2(0.0, 1.0), vec2(12.9898, 78.233))) * 43758.5453);
    return normalize(vec3(random * 2.0 - 1.0, random2 * 2.0 - 1.0, random3));
}

void main() {
    vec3 position = texture(g_Position, v_TexCoord).rgb;
    vec3 normal = normalize(texture(g_Normal, v_TexCoord).rgb);
    if (length(position) < 0.001) {
        FragColor = 1.0;
        return;
    }
    vec3 randomVec = RandomVec3(v_TexCoord);
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);
    float occlusion = 0.0;
    vec2 texelSize = 1.0 / pc.u_Resolution;
    int sampleCount = min(pc.u_SampleCount, 16);
    for (int i = 0; i < sampleCount; ++i) {
        vec3 samplePos = position + TBN * (sampleKernel[i] * pc.u_Radius);
        vec4 samplePosView = pc.u_ViewMatrix * vec4(samplePos, 1.0);
        vec4 offset = pc.u_ProjectionMatrix * samplePosView;
        offset.xy /= offset.w;
        offset.xy = offset.xy * 0.5 + 0.5;
        vec3 samplePosition = texture(g_Position, offset.xy).rgb;
        float sceneViewZ = (pc.u_ViewMatrix * vec4(samplePosition, 1.0)).z;
        float depthDiff = samplePosView.z - sceneViewZ;
        float distanceToSample = length(samplePos - position);
        float rangeCheck = smoothstep(0.0, 1.0, pc.u_Radius / distanceToSample);
        if (depthDiff < -pc.u_Bias && depthDiff > -pc.u_Radius)
            occlusion += rangeCheck;
    }
    occlusion = 1.0 - (occlusion / float(sampleCount));
    FragColor = occlusion;
}
