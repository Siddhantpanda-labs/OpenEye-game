#ifndef MAX_CASCADES
#define MAX_CASCADES 4
#endif

float CalculateShadow(vec3 fragPosWorldSpace, vec3 normal, vec3 lightDir,
                      sampler2D shadowMaps[MAX_CASCADES], mat4 shadowMatrices[MAX_CASCADES],
                      float cascadeSplits[MAX_CASCADES + 1], mat4 viewMatrix) {
    vec4 fragPosViewSpace = viewMatrix * vec4(fragPosWorldSpace, 1.0);
    float viewDepth = -fragPosViewSpace.z;
    int cascadeIndex = 0;
    for (int i = 0; i < MAX_CASCADES - 1; i++) if (viewDepth > cascadeSplits[i + 1]) cascadeIndex = i + 1;
    cascadeIndex = clamp(cascadeIndex, 0, MAX_CASCADES - 1);
    float blendWidth = 2.0;
    float visibility = 0.0;
    for (int c = cascadeIndex; c <= min(cascadeIndex + 1, MAX_CASCADES - 1); ++c) {
        float w = 1.0;
        if (c == cascadeIndex + 1) {
            float start = cascadeSplits[c] - blendWidth; float end = cascadeSplits[c] + blendWidth;
            w = clamp((viewDepth - start) / max(end - start, 0.0001), 0.0, 1.0);
        } else if (c == cascadeIndex && cascadeIndex < MAX_CASCADES - 1) {
            float start = cascadeSplits[c + 1] - blendWidth; float end = cascadeSplits[c + 1] + blendWidth;
            float t = clamp((viewDepth - start) / max(end - start, 0.0001), 0.0, 1.0); w = 1.0 - t;
        }
        vec4 fragPosLightSpace = shadowMatrices[c] * vec4(fragPosWorldSpace, 1.0);
        vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w; projCoords = projCoords * 0.5 + 0.5;
        if (projCoords.x < 0.0 || projCoords.x > 1.0 || projCoords.y < 0.0 || projCoords.y > 1.0 || projCoords.z > 1.0) { visibility += 1.0 * w; continue; }
        float bias = max(0.0015 * (1.0 - dot(normalize(normal), normalize(lightDir))), 0.0002);
        float lit = 0.0; float weightSum = 0.0; vec2 texelSize = 1.0 / textureSize(shadowMaps[c], 0);
        for (int x = -2; x <= 2; ++x) for (int y = -2; y <= 2; ++y) {
            vec2 offset = vec2(x, y) * texelSize; float dist = length(vec2(x, y)); float weight = exp(-(dist * dist) / 2.0);
            float pcfDepth = texture(shadowMaps[c], projCoords.xy + offset).r;
            lit += (projCoords.z - bias > pcfDepth ? 0.0 : 1.0) * weight; weightSum += weight;
        }
        lit = (weightSum > 0.0) ? lit / weightSum : 1.0; visibility += lit * w;
    }
    return clamp(visibility, 0.0, 1.0);
}


