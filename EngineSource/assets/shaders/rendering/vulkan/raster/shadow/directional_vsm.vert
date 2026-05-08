#version 450

layout(location = 0) in vec3 aPosition;

layout(push_constant) uniform ShadowPushConstants {
    mat4 uLightViewProj;
    mat4 uModel;
} pc;

void main() {
    gl_Position = pc.uLightViewProj * pc.uModel * vec4(aPosition, 1.0);
}
