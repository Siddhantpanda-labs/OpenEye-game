#version 450

layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;

layout(set = 0, binding = 1) uniform OutlineVertBlock {
    mat4 u_Model;
    mat4 u_View;
    mat4 u_Projection;
    float u_OutlineThickness;
};

void main() {
    vec4 worldPos = u_Model * vec4(a_Position, 1.0);
    vec3 worldNormal = mat3(transpose(inverse(u_Model))) * a_Normal;
    vec3 viewPos = (u_View * worldPos).xyz;
    vec3 viewNormal = normalize(mat3(u_View) * worldNormal);
    viewPos += viewNormal * u_OutlineThickness;
    gl_Position = u_Projection * vec4(viewPos, 1.0);
}
