// OpenEye next-gen rendering — M5 G-Buffer static fragment shader
// Logical: raster/deferred/gbuffer_static
#version 450

layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec3 vWorldN;
layout(location = 2) in vec2 vUv;
layout(location = 3) flat in uint vMaterialId;
layout(location = 4) flat in vec4 vBaseColor;
layout(location = 5) flat in vec3 vMrao;
layout(location = 6) flat in float vEmissiveStrength;
layout(location = 7) flat in float vPackedFlags;

layout(location = 0) out vec4 outRT0; // Albedo(RGB), packed flags(A)
layout(location = 1) out vec4 outRT1; // Normal(XYZ), emissiveStrength(A)
layout(location = 2) out vec4 outRT2; // Metallic(R), Roughness(G), AO(B), validMask(A)

void main() {
    vec3 N = normalize(vWorldN);
    vec3 albedo = vBaseColor.rgb;
    float metallic = clamp(vMrao.x, 0.0, 1.0);
    float roughness = clamp(vMrao.y, 0.04, 1.0);
    float ao = clamp(vMrao.z, 0.0, 1.0);

    // Write to G-Buffer
    outRT0 = vec4(albedo, vPackedFlags);
    outRT1 = vec4(N, clamp(vEmissiveStrength, 0.0, 16.0));
    outRT2 = vec4(metallic, roughness, ao, 1.0); // Mask=1 (valid material)
}
