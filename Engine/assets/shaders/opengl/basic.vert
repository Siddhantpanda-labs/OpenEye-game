#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

// Instance attributes (location 5-8 for model matrix)
// Instance attributes are ignored in this basic forward variant
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
    // Always use uniform model matrix (non-instanced forward path)
    mat4 modelMatrix = u_Model;
    
    v_Position = vec3(modelMatrix * vec4(a_Position, 1.0));
    v_Normal = mat3(transpose(inverse(modelMatrix))) * a_Normal;
    v_TexCoord = a_TexCoord;
    
    gl_Position = u_Projection * u_View * vec4(v_Position, 1.0);
}

