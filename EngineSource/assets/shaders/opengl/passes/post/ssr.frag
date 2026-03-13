#version 430 core

in vec2 v_TexCoord;
layout(location=0) out vec4 o_Color;

uniform sampler2D g_Position;
uniform sampler2D g_Normal;
uniform sampler2D g_Albedo;
uniform sampler2D g_PBRParams;
uniform sampler2D g_Depth;
uniform sampler2D u_SceneTexture;
uniform samplerCube u_Skybox;
uniform samplerCube u_ReflectionProbe;  // optional; fallback when SSR misses
uniform int u_HasReflectionProbe = 0;
uniform sampler2D u_PrevSSR;       // previous frame SSR result (temporal)
uniform int u_TemporalEnabled = 0;
uniform sampler2D u_DepthPyramid;  // Hi-Z min-depth pyramid (optional)
uniform int u_HasDepthPyramid = 0;
uniform int u_PyramidLevels = 8;
uniform vec2 u_Resolution;
uniform float u_StepSize = 0.1;
uniform int u_MaxSteps = 32;
uniform float u_Thickness = 0.01;
uniform float u_NearPlane = 0.1;
uniform float u_FarPlane = 1000.0;
uniform mat4 u_ViewMatrix;
uniform mat4 u_ProjectionMatrix;
uniform mat4 u_InverseViewMatrix;
uniform mat4 u_InverseProjectionMatrix;
uniform vec3 u_ViewPos;

// Linearize depth buffer value to view-space Z (negative in OpenGL view space)
float linearizeDepth(float depthSample) {
    float zNdc = depthSample * 2.0 - 1.0;
    float linear = (2.0 * u_NearPlane * u_FarPlane) / (u_FarPlane + u_NearPlane - zNdc * (u_FarPlane - u_NearPlane));
    return -linear;
}

// Ray march in view space: origin and direction in view space, project with projection matrix only.
// When Hi-Z pyramid is present, use it to skip empty regions (larger steps when ray is in front of cell min depth).
vec3 RayMarch(vec3 rayOriginVs, vec3 rayDirVs, float rayLength) {
    float stepSize = u_StepSize;
    int maxSteps = u_MaxSteps;

    vec3 currentPosVs = rayOriginVs;

    for (int i = 0; i < maxSteps; ++i) {
        currentPosVs += rayDirVs * stepSize;

        // Project view-space position to clip space then NDC
        vec4 clipPos = u_ProjectionMatrix * vec4(currentPosVs, 1.0);
        if (clipPos.w <= 0.0) break;
        vec2 ndc = clipPos.xy / clipPos.w;
        vec2 screenCoord = ndc * 0.5 + 0.5;

        if (screenCoord.x < 0.0 || screenCoord.x > 1.0 || screenCoord.y < 0.0 || screenCoord.y > 1.0) {
            break;
        }

        // Hi-Z: sample pyramid at coarser mip; if ray is in front of cell min depth, take larger step next time
        if (u_HasDepthPyramid != 0 && u_PyramidLevels > 0) {
            float mip = min(float(i / 4), float(u_PyramidLevels - 1));
            float cellMinViewZ = textureLod(u_DepthPyramid, screenCoord, mip).r;
            if (currentPosVs.z > cellMinViewZ)
                stepSize = min(stepSize * 1.5, u_StepSize * 4.0);
        }

        float sceneDepthSample = texture(g_Depth, screenCoord).r;
        float sceneViewZ = linearizeDepth(sceneDepthSample);

        // Skip sky (depth ~ 1.0 linearizes to far plane)
        if (sceneDepthSample >= 0.9999) continue;

        // Hit test: view-space Z comparison (both negative in OpenGL)
        float depthDiff = abs(currentPosVs.z - sceneViewZ);
        if (depthDiff < u_Thickness) {
            return texture(u_SceneTexture, screenCoord).rgb;
        }
    }

    return vec3(-1.0);
}

void main() {
    vec3 positionWs = texture(g_Position, v_TexCoord).rgb;
    vec3 normalWs = normalize(texture(g_Normal, v_TexCoord).rgb);
    vec3 sceneColor = texture(u_SceneTexture, v_TexCoord).rgb;

    if (length(positionWs) < 0.001) {
        o_Color = vec4(sceneColor, 1.0);
        return;
    }

    // World-space view and reflection direction
    vec3 viewDirWs = normalize(u_ViewPos - positionWs);
    vec3 reflectDirWs = reflect(-viewDirWs, normalWs);

    // Transform to view space for correct projection and depth test
    vec3 rayOriginVs = (u_ViewMatrix * vec4(positionWs, 1.0)).xyz;
    vec3 rayDirVs = normalize((u_ViewMatrix * vec4(reflectDirWs, 0.0)).xyz);

    vec3 reflectionColor = RayMarch(rayOriginVs, rayDirVs, 10.0);

    // Fallback hierarchy: 1) SSR hit, 2) Reflection probe, 3) Skybox
    if (reflectionColor.r < -0.5) {
        vec3 worldReflectDir = mat3(u_InverseViewMatrix) * reflectDirWs;
        if (u_HasReflectionProbe != 0)
            reflectionColor = texture(u_ReflectionProbe, worldReflectDir).rgb;
        else
            reflectionColor = texture(u_Skybox, worldReflectDir).rgb;
    }

    if (reflectionColor.r < -0.5) {
        reflectionColor = sceneColor;
    }

    float roughness = texture(g_PBRParams, v_TexCoord).g;
    vec3 skyColor = texture(u_Skybox, reflectDirWs).rgb;
    reflectionColor = mix(reflectionColor, skyColor, roughness);

    // Edge fading: remove cut-off artifact near screen edges
    float edgeFade = smoothstep(0.0, 0.1, v_TexCoord.x) * smoothstep(0.0, 0.1, v_TexCoord.y)
                   * smoothstep(0.0, 0.1, 1.0 - v_TexCoord.x) * smoothstep(0.0, 0.1, 1.0 - v_TexCoord.y);
    reflectionColor *= edgeFade;

    // BRDF weighting: PBR Fresnel for reflection weight (F0 = mix(0.04, albedo, metallic))
    vec3 albedo = texture(g_Albedo, v_TexCoord).rgb;
    float metallic = texture(g_PBRParams, v_TexCoord).r;
    float NdotV = max(dot(normalWs, viewDirWs), 0.0);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = F0 + (1.0 - F0) * pow(1.0 - NdotV, 5.0);
    vec3 reflectionTerm = F * reflectionColor;

    float reflective = texture(g_PBRParams, v_TexCoord).b;
    vec3 finalColor = mix(sceneColor, reflectionTerm, reflective);

    // Temporal accumulation: blend with previous frame to stabilize reflections
    if (u_TemporalEnabled != 0) {
        vec3 prevColor = texture(u_PrevSSR, v_TexCoord).rgb;
        finalColor = mix(prevColor, finalColor, 0.2);
    }

    o_Color = vec4(finalColor, 1.0);
}
