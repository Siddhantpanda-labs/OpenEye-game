// OpenEye next-gen rendering — M5 G-Buffer static fragment shader
// Logical: raster/deferred/gbuffer_static
#version 450

layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec3 vWorldN;
layout(location = 2) in vec2 vUv;
layout(location = 3) flat in uint vMaterialId;

layout(location = 0) out vec4 outRT0; // Albedo(RGB), Flags(A)
layout(location = 1) out vec4 outRT1; // Normal(XYZ), Unused(A)
layout(location = 2) out vec4 outRT2; // Metallic(R), Roughness(G), AO(B), Mask(A)

void main() {
    vec3 N = normalize(vWorldN);
    
    vec3 albedo = vec3(0.72, 0.45, 0.32);
    if (vMaterialId == 3u)
        albedo = vec3(1.0, 0.92, 0.18); // bright yellow debug quad
    else if ((vMaterialId & 1u) != 0u)
        albedo = vec3(0.32, 0.62, 0.88);

    // Write to G-Buffer
    outRT0 = vec4(albedo, 0.0); // Flags=0
    outRT1 = vec4(N, 1.0);
    outRT2 = vec4(0.0, 0.8, 1.0, 1.0); // Metallic 0, Roughness 0.8, AO 1
}
