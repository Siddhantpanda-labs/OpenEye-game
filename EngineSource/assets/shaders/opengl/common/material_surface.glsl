// Shared surface evaluation for all PBR pipeline templates (GBuffer, Forward, Forward+).
// Single source of truth for material sampling and normal mapping; pipelines use surf for their output.

struct MaterialSurface {
    vec3 albedo;
    vec3 normal;
    float metallic;
    float roughness;
    float ao;
    vec3 emission;
    float alpha;
    float reflective;
};

void EvaluateMaterialSurface(
    vec2 uv,
    mat3 tbn,
    vec3 vertexNormal,
    vec3 albedoBase,
    float metallicBase,
    float roughnessBase,
    float aoBase,
    vec3 emissionBase,
    float alphaBase,
    float reflectiveBase,
    sampler2D albedoTex,
    sampler2D normalTex,
    sampler2D mrTex,
    sampler2D metallicTex,
    sampler2D roughnessTex,
    sampler2D aoTex,
    sampler2D emissiveTex,
    bool useAlbedo,
    bool useNormal,
    bool useMR,
    bool useMetallic,
    bool useRoughness,
    bool useAO,
    bool useEmissive,
    out MaterialSurface surf
) {
    surf.albedo = albedoBase;
    surf.alpha = alphaBase;
    if (useAlbedo) {
        vec4 texel = texture(albedoTex, uv);
        surf.albedo *= texel.rgb;
        surf.alpha *= texel.a;
    }
    surf.albedo = clamp(surf.albedo, vec3(0.0), vec3(1.0));

    surf.normal = normalize(vertexNormal);
    if (useNormal) {
        vec3 n = texture(normalTex, uv).rgb * 2.0 - 1.0;
        surf.normal = normalize(tbn * n);
    }
    if (isnan(surf.normal.x) || isnan(surf.normal.y) || isnan(surf.normal.z))
        surf.normal = vec3(0.0, 1.0, 0.0);

    surf.metallic = metallicBase;
    surf.roughness = roughnessBase;
    if (useMR) {
        vec2 mr = texture(mrTex, uv).bg;
        surf.metallic = mr.x;
        surf.roughness = mr.y;
    } else {
        if (useMetallic) surf.metallic = texture(metallicTex, uv).r;
        if (useRoughness) surf.roughness = texture(roughnessTex, uv).r;
    }
    surf.metallic = clamp(surf.metallic, 0.0, 1.0);
    surf.roughness = clamp(surf.roughness, 0.04, 1.0);

    surf.ao = useAO ? texture(aoTex, uv).r : aoBase;
    surf.emission = useEmissive ? (emissionBase + texture(emissiveTex, uv).rgb) : emissionBase;
    surf.reflective = clamp(reflectiveBase, 0.0, 1.0);
}
