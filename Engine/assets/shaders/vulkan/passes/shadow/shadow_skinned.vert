#version 460

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 7) in vec2 a_TexCoord1;
layout(location = 3) in vec3 a_Tangent;
layout(location = 4) in vec4 a_Color;
layout(location = 5) in ivec4 a_BoneIndices;
layout(location = 6) in vec4 a_BoneWeights;

// set 1 = ShadowPass (binding 0 = cascade UBO)
layout(set = 1, binding = 0) uniform ShadowBlock {
    mat4 u_LightSpaceMatrix;
};
// set 2 = SceneBuffers (binding 1 = bones SSBO)
layout(set = 2, binding = 1) readonly buffer Bones {
    mat4 u_Bones[];
};

layout(push_constant, std430) uniform Push {
    mat4 u_Model;
} pc;

void main() {
    vec4 pos = vec4(a_Position, 1.0);
    mat4 skin = mat4(0.0);
    for (int i = 0; i < 4; ++i) {
        int idx = a_BoneIndices[i];
        float w = a_BoneWeights[i];
        if (w > 0.0 && idx >= 0)
            skin += u_Bones[idx] * w;
    }
    pos = skin * pos;
    vec4 worldPos = pc.u_Model * pos;
    gl_Position = u_LightSpaceMatrix * worldPos;
}
