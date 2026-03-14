// Single source of truth for per-material UBO (binding 2). Used by forward and deferred passes.
#ifndef MATERIAL_BLOCK_GLSL
#define MATERIAL_BLOCK_GLSL
layout(std140, binding = 2) uniform MaterialParams {
    vec3 albedo;
    float metallic;
    float roughness;
    float ao;
    float alpha;
    vec3 emission;
    float reflective;
} MaterialBlock;
#endif
