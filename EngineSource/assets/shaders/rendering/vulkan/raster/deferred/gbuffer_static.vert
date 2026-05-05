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

struct MaterialRow {
    vec4 baseColorFactor;
    vec4 emissiveFactor;
    float metallicFactor;
    float roughnessFactor;
    float normalScale;
    float occlusionStrength;
    float alphaCutoff;
    uint alphaMode;
    uint doubleSided;
    uint _pad0;
    uvec4 textureHandles01;
    uvec4 textureHandles23;
};

layout(std430, set = 1, binding = 1) readonly buffer MaterialTableSsbo {
    MaterialRow rows[];
} gMaterialTable;

layout(push_constant) uniform Push {
    uint objectIndex;
} pc;

layout(location = 0) out vec3 vWorldPos;
layout(location = 1) out vec3 vWorldN;
layout(location = 2) out vec2 vUv;
layout(location = 3) flat out uint vMaterialId;
layout(location = 4) flat out vec4 vBaseColor;
layout(location = 5) flat out vec3 vMrao;
layout(location = 6) flat out float vEmissiveStrength;
layout(location = 7) flat out float vPackedFlags;

void main() {
    PerObjectRow row = gPerObject.items[pc.objectIndex];
    MaterialRow mat = gMaterialTable.rows[row.materialId];
    vec4 worldPos = row.model * vec4(inPosition, 1.0);
    mat3 nMat = mat3(transpose(inverse(row.model)));
    vWorldPos = worldPos.xyz;
    vWorldN = normalize(nMat * inNormal);
    vUv = inUv;
    vMaterialId = row.materialId;
    vBaseColor = mat.baseColorFactor;
    vMrao = vec3(mat.metallicFactor, mat.roughnessFactor, mat.occlusionStrength);
    vEmissiveStrength = max(max(mat.emissiveFactor.r, mat.emissiveFactor.g), mat.emissiveFactor.b);
    uint flags = (mat.alphaMode & 0x3u) | ((mat.doubleSided & 0x1u) << 2u);
    vPackedFlags = float(flags) / 255.0;
    gl_Position = uPerView.uProjection * uPerView.uView * worldPos;
}
