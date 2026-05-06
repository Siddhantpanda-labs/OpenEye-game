// OpenEye next-gen rendering — M5 G-Buffer static fragment shader
// Logical: raster/deferred/gbuffer_static
#version 450
#extension GL_EXT_nonuniform_qualifier : require

layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec3 vWorldN;
layout(location = 2) in vec2 vUv;
layout(location = 3) flat in uint vMaterialId;
layout(location = 4) flat in vec4 vBaseColor;
layout(location = 5) flat in vec3 vMrao;
layout(location = 6) flat in float vEmissiveStrength;
layout(location = 7) flat in float vPackedFlags;
layout(location = 8) flat in uvec4 vTextureHandles01;
layout(location = 9) flat in uvec4 vTextureHandles23;

layout(set = 2, binding = 0) uniform sampler2D uBindlessTextures[];

layout(location = 0) out vec4 outRT0; // Albedo(RGB), packed flags(A)
layout(location = 1) out vec4 outRT1; // Normal(XYZ), emissiveStrength(A)
layout(location = 2) out vec4 outRT2; // Metallic(R), Roughness(G), AO(B), validMask(A)

const uint kInvalidHandle = 0xFFFFFFFFu;

vec4 SampleBindless2D(uint handle, vec2 uv, vec4 fallbackValue) {
    if (handle == kInvalidHandle || handle == 0u)
        return fallbackValue;
    return texture(uBindlessTextures[nonuniformEXT(int(handle - 1u))], uv);
}

void main() {
    vec3 N = normalize(vWorldN);
    vec3 albedo = vBaseColor.rgb;
    float metallic = clamp(vMrao.x, 0.0, 1.0);
    float roughness = clamp(vMrao.y, 0.04, 1.0);
    float ao = clamp(vMrao.z, 0.0, 1.0);
    float emissiveStrength = clamp(vEmissiveStrength, 0.0, 16.0);

    // Material texture handles are authored by CPU material-table packing.
    const uint baseColorHandle = vTextureHandles01.x;
    const uint normalHandle = vTextureHandles01.y;
    const uint metallicRoughnessHandle = vTextureHandles01.z;
    const uint emissiveHandle = vTextureHandles01.w;
    const uint occlusionHandle = vTextureHandles23.x;

    vec4 baseTex = SampleBindless2D(baseColorHandle, vUv, vec4(1.0));
    albedo *= baseTex.rgb;

    vec4 mrTex = SampleBindless2D(metallicRoughnessHandle, vUv, vec4(1.0));
    metallic *= clamp(mrTex.b, 0.0, 1.0);
    roughness *= clamp(mrTex.g, 0.04, 1.0);

    vec4 aoTex = SampleBindless2D(occlusionHandle, vUv, vec4(1.0));
    ao *= clamp(aoTex.r, 0.0, 1.0);

    vec3 mappedN = normalize(N);
    if (normalHandle != kInvalidHandle && normalHandle != 0u) {
        vec3 tangentN = SampleBindless2D(normalHandle, vUv, vec4(0.5, 0.5, 1.0, 1.0)).xyz * 2.0 - 1.0;
        tangentN = normalize(tangentN);
        vec3 up = (abs(N.z) < 0.999) ? vec3(0.0, 0.0, 1.0) : vec3(0.0, 1.0, 0.0);
        vec3 T = normalize(cross(up, N));
        vec3 B = cross(N, T);
        mat3 tbn = mat3(T, B, N);
        mappedN = normalize(tbn * tangentN);
    }

    vec3 emissiveTex = SampleBindless2D(emissiveHandle, vUv, vec4(0.0)).rgb;
    emissiveStrength = max(emissiveStrength, max(max(emissiveTex.r, emissiveTex.g), emissiveTex.b));

    // Write to G-Buffer
    outRT0 = vec4(albedo, vPackedFlags);
    outRT1 = vec4(mappedN, emissiveStrength);
    outRT2 = vec4(metallic, roughness, ao, 1.0); // Mask=1 (valid material)
}
