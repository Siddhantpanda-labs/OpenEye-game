#version 450 core

// Canonical mesh layout (all declared; position + bones used)
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 7) in vec2 a_TexCoord1;
layout(location = 3) in vec3 a_Tangent;
layout(location = 4) in vec4 a_Color;
layout(location = 5) in ivec4 a_BoneIndices;
layout(location = 6) in vec4 a_BoneWeights;

uniform mat4 u_Model;
uniform mat4 u_LightSpaceMatrix;

#define MAX_BONES 128
layout(std140, binding = 3) uniform Bones {
    mat4 u_Bones[MAX_BONES];
};

void main() {
    vec4 pos = vec4(a_Position, 1.0);
    mat4 skin = mat4(0.0);
    for (int i = 0; i < 4; ++i) {
        int idx = a_BoneIndices[i];
        float w = a_BoneWeights[i];
        if (w > 0.0 && idx >= 0 && idx < MAX_BONES) {
            skin += u_Bones[idx] * w;
        }
    }
    pos = skin * pos;
    vec4 worldPos = u_Model * pos;
    gl_Position = u_LightSpaceMatrix * worldPos;
}
