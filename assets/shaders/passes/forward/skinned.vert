#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 5) in ivec4 a_BoneIndices;
layout(location = 6) in vec4 a_BoneWeights;

uniform mat4 u_Model;
uniform mat4 u_View;
uniform mat4 u_Projection;

#define MAX_BONES 512
layout(std140, binding = 3) uniform Bones {
    mat4 u_Bones[MAX_BONES];
};

out vec3 v_Position;
out vec3 v_Normal;
out vec2 v_TexCoord;

void main() {
    vec4 pos = vec4(a_Position, 1.0);
    vec3 nrm = a_Normal;

    float weightSum = dot(a_BoneWeights, vec4(1.0));
    if (weightSum > 0.0001) {
        mat4 skin = mat4(0.0);
        for (int i = 0; i < 4; ++i) {
            int idx = a_BoneIndices[i];
            float w = a_BoneWeights[i];
            if (w > 0.0 && idx >= 0 && idx < MAX_BONES) {
                skin += u_Bones[idx] * w;
            }
        }
        pos = skin * pos;
        nrm = mat3(skin) * nrm;
    }

    vec4 worldPos = u_Model * pos;
    v_Position = worldPos.xyz;
    v_Normal = mat3(transpose(inverse(u_Model))) * nrm;
    v_TexCoord = a_TexCoord;
    gl_Position = u_Projection * u_View * worldPos;
}
