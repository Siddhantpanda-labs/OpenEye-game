#version 460 core

// Canonical mesh layout (all declared)
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 7) in vec2 a_TexCoord1;
layout(location = 3) in vec3 a_Tangent;
layout(location = 4) in vec4 a_Color;
layout(location = 5) in ivec4 a_BoneIndices;
layout(location = 6) in vec4 a_BoneWeights;

uniform mat4 u_Model;
uniform mat4 u_View;
uniform mat4 u_Projection;
uniform mat4 u_PrevView;
uniform mat4 u_PrevProjection;
// GPU-driven scene path: per-draw model matrices (binding 4); use when u_UsePerDrawTransformBuffer != 0
layout(std430, binding = 4) readonly buffer PerDrawModel {
    mat4 u_ModelArray[];
};
uniform int u_UsePerDrawTransformBuffer = 0;

uniform vec4 u_UVTransform = vec4(0.0, 0.0, 1.0, 1.0);
uniform float u_UVRotation = 0.0;
uniform float u_TexCoordIndex = 0.0;
uniform float u_FlipV = 1.0;

#define MAX_BONES 128
layout(std140, binding = 3) uniform Bones {
    mat4 u_Bones[MAX_BONES];
};

out vec3 v_WorldPos;
out vec3 v_WorldNormal;
out vec2 v_TexCoord;
out vec2 v_Velocity;
out mat3 v_TBN;

void main() {
    vec4 pos = vec4(a_Position, 1.0);
    vec3 nrm = a_Normal;
    vec3 tang = a_Tangent;

    // Skinning (same as forward skinned.vert)
    mat4 skin = mat4(0.0);
    float totalWeight = 0.0;
    for (int i = 0; i < 4; ++i) {
        int idx = a_BoneIndices[i];
        float w = a_BoneWeights[i];
        if (w > 0.0 && idx >= 0 && idx < MAX_BONES) {
            skin += u_Bones[idx] * w;
            totalWeight += w;
        }
    }
    if (totalWeight <= 0.0) {
        // Non-skinned mesh: use identity skinning
        skin = mat4(1.0);
    }
    pos = skin * pos;
    nrm = mat3(skin) * nrm;
    tang = mat3(skin) * tang;

    mat4 model = (u_UsePerDrawTransformBuffer != 0) ? u_ModelArray[gl_DrawID] : u_Model;
    vec4 worldPos = model * pos;
    mat3 normalMatrix = mat3(transpose(inverse(model)));
    vec3 worldNormal = normalize(normalMatrix * nrm);
    vec3 worldTangent;
    if (length(tang) < 0.001) {
        // No tangent: build TBN from normal (e.g. for normal map in world space fallback)
        vec3 up = abs(worldNormal.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
        worldTangent = normalize(cross(up, worldNormal));
    } else {
        worldTangent = normalize(normalMatrix * tang);
    }

    v_WorldPos = worldPos.xyz;
    v_WorldNormal = worldNormal;
    // UV transform (same as forward)
    vec2 baseUv = mix(a_TexCoord, a_TexCoord1, step(0.5, u_TexCoordIndex));
    vec2 uv = baseUv * u_UVTransform.zw;
    float c = cos(u_UVRotation);
    float s = sin(u_UVRotation);
    uv = mat2(c, -s, s, c) * uv;
    uv += u_UVTransform.xy;
    if (u_FlipV > 0.5) uv.y = 1.0 - uv.y;
    v_TexCoord = uv;

    // Velocity (same as gbuffer.vert)
    vec4 currentPos = u_Projection * u_View * worldPos;
    vec4 prevPos = u_PrevProjection * u_PrevView * worldPos;
    vec2 currentNDC = currentPos.xy / currentPos.w;
    vec2 prevNDC = prevPos.xy / prevPos.w;
    v_Velocity = currentNDC - prevNDC;

    // TBN for normal mapping
    vec3 T = normalize(worldTangent - dot(worldTangent, worldNormal) * worldNormal);
    vec3 B = cross(worldNormal, T);
    v_TBN = mat3(T, B, worldNormal);

    gl_Position = currentPos;
}
