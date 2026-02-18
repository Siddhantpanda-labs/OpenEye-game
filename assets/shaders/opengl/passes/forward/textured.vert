#version 460 core

// Canonical mesh layout (all declared; pos/normal/uv used)
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

out vec3 v_Position;
out vec3 v_Normal;
out vec2 v_TexCoord;

void main() {
    vec4 worldPos = u_Model * vec4(a_Position, 1.0);
    v_Position = worldPos.xyz;
    v_Normal = mat3(transpose(inverse(u_Model))) * a_Normal;
    // Match Vulkan engine: flip V to account for image origin conventions
    v_TexCoord = vec2(a_TexCoord.x, 1.0 - a_TexCoord.y);
    gl_Position = u_Projection * u_View * worldPos;
}
