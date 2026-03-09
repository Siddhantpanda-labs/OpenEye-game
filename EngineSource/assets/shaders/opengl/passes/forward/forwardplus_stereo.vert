#version 460 core

// Single-pass stereo: one draw, two eyes. gl_InstanceID selects eye; gl_ViewportIndex routes to left/right half.

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 7) in vec2 a_TexCoord1;
layout(location = 3) in vec3 a_Tangent;
layout(location = 4) in vec4 a_Color;
layout(location = 5) in ivec4 a_BoneIndices;
layout(location = 6) in vec4 a_BoneWeights;

uniform mat4 u_Model;
uniform mat4 u_View[2];
uniform mat4 u_Projection[2];
uniform vec3 u_ViewPos[2];
uniform vec4 u_UVTransform;
uniform float u_UVRotation;
uniform float u_TexCoordIndex;
uniform float u_FlipV;

out vec3 v_WorldPos;
out vec3 v_ViewPos;
out vec3 v_WorldNormal;
out vec2 v_TexCoord;
out mat3 v_TBN;
flat out vec3 v_CameraWorldPos;

void main() {
    int eye = int(gl_InstanceID);
    eye = clamp(eye, 0, 1);
    gl_ViewportIndex = eye;

    vec4 w = u_Model * vec4(a_Position, 1.0);
    v_WorldPos = w.xyz;
    vec4 viewPos = u_View[eye] * w;
    v_ViewPos = viewPos.xyz;
    v_CameraWorldPos = u_ViewPos[eye];

    mat3 normalMatrix = mat3(transpose(inverse(u_Model)));
    vec3 worldNormal = normalize(normalMatrix * a_Normal);
    vec3 worldTangent = a_Tangent;
    if (length(worldTangent) < 0.001) {
        vec3 up = abs(worldNormal.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
        worldTangent = normalize(cross(up, worldNormal));
    } else {
        worldTangent = normalize(normalMatrix * worldTangent);
    }
    vec3 worldBitangent = normalize(cross(worldNormal, worldTangent));
    v_WorldNormal = worldNormal;
    v_TBN = mat3(worldTangent, worldBitangent, worldNormal);

    vec2 uv = a_TexCoord * u_UVTransform.zw;
    float c = cos(u_UVRotation);
    float s = sin(u_UVRotation);
    uv = mat2(c, -s, s, c) * uv;
    uv += u_UVTransform.xy;
    if (u_FlipV > 0.5) {
        uv.y = 1.0 - uv.y;
    }
    v_TexCoord = uv;
    gl_Position = u_Projection[eye] * viewPos;
}
