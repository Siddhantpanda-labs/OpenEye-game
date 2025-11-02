#version 450 core

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

uniform mat4 u_Model;

#include "../../common/camera.glsl"

out vec3 v_Position;
out vec3 v_Normal;
out vec2 v_TexCoord;

void main() {
    mat4 modelMatrix = u_Model;
    vec4 worldPos = modelMatrix * vec4(a_Position, 1.0);
    v_Position = worldPos.xyz;
    v_Normal = mat3(transpose(inverse(modelMatrix))) * a_Normal;
    v_TexCoord = a_TexCoord;
    gl_Position = u_ProjectionMatrix * u_ViewMatrix * worldPos;
}


