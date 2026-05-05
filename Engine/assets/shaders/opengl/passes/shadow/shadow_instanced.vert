#version 430 core

// Instanced shadow: per-instance model matrix from SSBO (Limitless model_buffer style).
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 7) in vec2 a_TexCoord1;
layout(location = 3) in vec3 a_Tangent;
layout(location = 4) in vec4 a_Color;
layout(location = 5) in ivec4 a_BoneIndices;
layout(location = 6) in vec4 a_BoneWeights;

layout(std430, binding = 4) readonly buffer InstanceModels {
    mat4 u_InstanceModel[];
};

uniform mat4 u_LightSpaceMatrix;

void main() {
    mat4 u_Model = u_InstanceModel[gl_InstanceID];
    vec4 worldPos = u_Model * vec4(a_Position, 1.0);
    gl_Position = u_LightSpaceMatrix * worldPos;
}
