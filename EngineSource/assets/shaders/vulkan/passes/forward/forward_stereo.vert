#version 460
// Stereo preview entry: currently matches forward.vert so PreferSpirv can load forward_stereo.*.spv.
// Full single-pass stereo on Vulkan needs viewport index + dual view/proj in push/UBO and CommandListVK wiring.

// Resource-space: set 0 = Frame (camera UBO). Push: model, UV.

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 7) in vec2 a_TexCoord1;
layout(location = 3) in vec3 a_Tangent;
layout(location = 4) in vec4 a_Color;
layout(location = 5) in ivec4 a_BoneIndices;
layout(location = 6) in vec4 a_BoneWeights;

layout(location = 0) out vec3 v_Position;
layout(location = 1) out vec3 v_Normal;
layout(location = 2) out vec2 v_TexCoord;
layout(location = 3) out mat3 v_TBN;

// Set 0 — Frame
layout(set = 0, binding = 0) uniform FrameBlock {
    mat4 u_View;
    mat4 u_Projection;
    mat4 u_ViewProj;
};

layout(push_constant, std430) uniform ForwardPush {
    mat4 u_Model;
    vec4 u_UVTransform;
    float u_UVRotation;
    float u_TexCoordIndex;
    float u_FlipV;
} pc;

void main() {
    vec4 w = pc.u_Model * vec4(a_Position, 1.0);
    v_Position = w.xyz;
    mat3 normalMatrix = mat3(transpose(inverse(pc.u_Model)));
    vec3 worldNormal = normalize(normalMatrix * a_Normal);
    vec3 worldTangent = length(a_Tangent) > 0.001
        ? normalize(normalMatrix * a_Tangent)
        : (abs(worldNormal.y) < 0.999 ? normalize(cross(vec3(0.0, 1.0, 0.0), worldNormal)) : normalize(cross(vec3(1.0, 0.0, 0.0), worldNormal)));
    vec3 T = normalize(worldTangent - dot(worldTangent, worldNormal) * worldNormal);
    v_TBN = mat3(T, cross(worldNormal, T), worldNormal);
    v_Normal = worldNormal;
    vec2 baseUv = mix(a_TexCoord, a_TexCoord1, step(0.5, pc.u_TexCoordIndex));
    vec2 uv = baseUv * pc.u_UVTransform.zw;
    float c = cos(pc.u_UVRotation);
    float s = sin(pc.u_UVRotation);
    uv = mat2(c, -s, s, c) * uv + pc.u_UVTransform.xy;
    if (pc.u_FlipV > 0.5) uv.y = 1.0 - uv.y;
    v_TexCoord = uv;
    gl_Position = u_Projection * u_View * w;
}
