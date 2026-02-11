#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

uniform mat4 u_Model;
uniform mat4 u_View;
uniform mat4 u_Projection;

out vec3 v_Position;
out vec3 v_Normal;
out vec2 v_TexCoord;

void main() {
    mat4 modelMatrix = u_Model;
    v_Position = vec3(modelMatrix * vec4(a_Position, 1.0));
    v_Normal = mat3(transpose(inverse(modelMatrix))) * a_Normal;
    // Match Vulkan engine: flip V to account for image origin conventions
    v_TexCoord = vec2(a_TexCoord.x, 1.0 - a_TexCoord.y);
    gl_Position = u_Projection * u_View * vec4(v_Position, 1.0);
}


