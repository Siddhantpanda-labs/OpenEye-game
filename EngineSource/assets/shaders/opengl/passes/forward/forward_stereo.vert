#version 460 core
#extension GL_ARB_shader_viewport_layer_array : require

// Single-pass stereo from forward/lit: one draw, two eyes. gl_InstanceID selects eye; gl_ViewportIndex routes to left/right half.

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

out vec3 v_Position;
out vec3 v_Normal;
out vec2 v_TexCoord;
flat out vec3 v_CameraWorldPos;

void main() {
    int eye = int(gl_InstanceID);
    eye = clamp(eye, 0, 1);
    gl_ViewportIndex = eye;

    mat4 modelMatrix = u_Model;
    vec4 worldPos = modelMatrix * vec4(a_Position, 1.0);
    v_Position = vec3(worldPos);
    v_Normal = mat3(transpose(inverse(modelMatrix))) * a_Normal;
    v_TexCoord = vec2(a_TexCoord.x, 1.0 - a_TexCoord.y);
    v_CameraWorldPos = u_ViewPos[eye];
    gl_Position = u_Projection[eye] * u_View[eye] * worldPos;
}
