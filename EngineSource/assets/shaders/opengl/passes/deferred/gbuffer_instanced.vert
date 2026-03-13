#version 460 core

// Instanced GBuffer: per-instance model matrix from SSBO (Limitless model_buffer style).
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 7) in vec2 a_TexCoord1;
layout(location = 3) in vec3 a_Tangent;
layout(location = 4) in vec4 a_Color;
layout(location = 5) in ivec4 a_BoneIndices;
layout(location = 6) in vec4 a_BoneWeights;

// Instance matrices: binding 4 (SSBO), one mat4 per instance
layout(std430, binding = 4) readonly buffer InstanceModels {
    mat4 u_InstanceModel[];
};

uniform mat4 u_View;
uniform mat4 u_Projection;
uniform mat4 u_PrevView;
uniform mat4 u_PrevProjection;
uniform vec4 u_UVTransform = vec4(0.0, 0.0, 1.0, 1.0);
uniform float u_UVRotation = 0.0;
uniform float u_TexCoordIndex = 0.0;
uniform float u_FlipV = 1.0;

out vec3 v_WorldPos;
out vec3 v_WorldNormal;
out vec2 v_TexCoord;
out vec2 v_Velocity;
out mat3 v_TBN;

void main() {
    mat4 u_Model = u_InstanceModel[gl_InstanceID];
    vec4 w = u_Model * vec4(a_Position, 1.0);
    mat3 normalMatrix = mat3(transpose(inverse(u_Model)));
    vec3 worldNormal = normalize(normalMatrix * a_Normal);
    vec3 worldTangent = length(a_Tangent) > 0.001 ? normalize(normalMatrix * a_Tangent) : (abs(worldNormal.y) < 0.999 ? normalize(cross(vec3(0, 1, 0), worldNormal)) : normalize(cross(vec3(1, 0, 0), worldNormal)));
    vec3 T = normalize(worldTangent - dot(worldTangent, worldNormal) * worldNormal);
    vec3 B = cross(worldNormal, T);
    v_TBN = mat3(T, B, worldNormal);

    v_WorldPos = w.xyz;
    v_WorldNormal = worldNormal;
    vec2 baseUv = mix(a_TexCoord, a_TexCoord1, step(0.5, u_TexCoordIndex));
    vec2 uv = baseUv * u_UVTransform.zw;
    float c = cos(u_UVRotation);
    float s = sin(u_UVRotation);
    uv = mat2(c, -s, s, c) * uv;
    uv += u_UVTransform.xy;
    if (u_FlipV > 0.5) uv.y = 1.0 - uv.y;
    v_TexCoord = uv;

    vec4 currentPos = u_Projection * u_View * w;
    vec4 prevPos = u_PrevProjection * u_PrevView * w;
    vec2 currentNDC = currentPos.xy / currentPos.w;
    vec2 prevNDC = prevPos.xy / prevPos.w;
    v_Velocity = currentNDC - prevNDC;

    gl_Position = currentPos;
}
