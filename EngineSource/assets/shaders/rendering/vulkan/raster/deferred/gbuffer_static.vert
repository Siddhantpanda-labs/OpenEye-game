// OpenEye next-gen rendering — M5 G-Buffer static vertex shader
// Logical: raster/deferred/gbuffer_static
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUv;

layout(set = 0, binding = 1) uniform PerViewUbo {
    mat4 uView;
    mat4 uProjection;
    mat4 uViewProjection;
    vec4 uViewPos;
    vec4 uViewport;
    vec4 uViewPad0;
    vec4 uViewPad1;
} uPerView;

struct PerObjectRow {
    mat4 model;
    uint materialId;
    uint meshHandle;
    uint transformIndex;
    uint _padRow;
};

layout(std430, set = 1, binding = 0) readonly buffer PerObjectSsbo {
    PerObjectRow items[];
} gPerObject;

layout(push_constant) uniform Push {
    uint objectIndex;
} pc;

layout(location = 0) out vec3 vWorldPos;
layout(location = 1) out vec3 vWorldN;
layout(location = 2) out vec2 vUv;
layout(location = 3) flat out uint vMaterialId;

void main() {
    PerObjectRow row = gPerObject.items[pc.objectIndex];
    vec4 worldPos = row.model * vec4(inPosition, 1.0);
    mat3 nMat = mat3(transpose(inverse(row.model)));
    vWorldPos = worldPos.xyz;
    vWorldN = normalize(nMat * inNormal);
    vUv = inUv;
    vMaterialId = row.materialId;
    gl_Position = uPerView.uProjection * uPerView.uView * worldPos;
}
