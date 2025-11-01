#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

// Instance attributes (location 5-8 for model matrix)
// Instance attributes (unused in this non-instanced variant)
layout(location = 5) in vec4 a_InstanceModel0;
layout(location = 6) in vec4 a_InstanceModel1;
layout(location = 7) in vec4 a_InstanceModel2;
layout(location = 8) in vec4 a_InstanceModel3;

uniform mat4 u_Model;           // Used for non-instanced rendering
uniform mat4 u_View;
uniform mat4 u_Projection;

out vec3 v_Position;
out vec3 v_Normal;
out vec2 v_TexCoord;

void main() {
    // Always use uniform model matrix for now (non-instanced geometry pass)
    mat4 modelMatrix = u_Model;
    
    vec4 worldPos = modelMatrix * vec4(a_Position, 1.0);
    v_Position = worldPos.xyz;
    v_Normal = mat3(transpose(inverse(modelMatrix))) * a_Normal;
    v_TexCoord = a_TexCoord;
    
    gl_Position = u_Projection * u_View * worldPos;
}

