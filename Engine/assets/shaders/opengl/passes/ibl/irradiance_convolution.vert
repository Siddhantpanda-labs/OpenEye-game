#version 450 core

// Canonical mesh layout (all declared; only a_Position used)
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 7) in vec2 a_TexCoord1;
layout(location = 3) in vec3 a_Tangent;
layout(location = 4) in vec4 a_Color;
layout(location = 5) in ivec4 a_BoneIndices;
layout(location = 6) in vec4 a_BoneWeights;

out vec3 v_WorldPos;

uniform mat4 u_Projection;
uniform mat4 u_View;

void main() {
    v_WorldPos = a_Position;
    mat4 rotView = mat4(mat3(u_View)); // Remove translation for skybox
    vec4 clipPos = u_Projection * rotView * vec4(a_Position, 1.0);
    gl_Position = clipPos.xyww; // Force z to w so after perspective divide z = 1.0 (far plane)
}

