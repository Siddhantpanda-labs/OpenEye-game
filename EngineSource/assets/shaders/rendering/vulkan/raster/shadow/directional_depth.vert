#version 450

layout(location = 0) in vec3 aPosition;

layout(std430, set = 1, binding = 0) readonly buffer ShadowCasterTransformsSsbo {
    mat4 transforms[];
} gTransforms;

layout(push_constant) uniform ShadowPushConstants {
    mat4 uLightViewProj;
    uint baseObjectIndex;
} pc;

void main() {
    uint objIdx = pc.baseObjectIndex + gl_InstanceIndex;
    mat4 uModel = gTransforms.transforms[objIdx];
    gl_Position = pc.uLightViewProj * uModel * vec4(aPosition, 1.0);
}
