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

// Ray march in view space: origin and direction in view space, project with projection matrix only
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

        float sceneDepthSample = texture(g_Depth, screenCoord).r;
        float sceneViewZ = linearizeDepth(sceneDepthSample);

        // Skip sky (depth ~ 1.0 linearizes to far plane)
        if (sceneDepthSample >= 0.9999) continue;

        // Hit test: view-space Z comparison (both negative in OpenGL)
        float depthDiff = abs(currentPosVs.z - sceneViewZ);
        if (depthDiff < u_Thickness) {
            return texture(g_Albedo, screenCoord).rgb;
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

    if (reflectionColor.r < -0.5) {
        vec3 worldReflectDir = mat3(u_InverseViewMatrix) * reflectDirWs;
        reflectionColor = texture(u_Skybox, worldReflectDir).rgb;
    }

    if (reflectionColor.r < -0.5) {
        reflectionColor = sceneColor;
    }

    float reflective = texture(g_PBRParams, v_TexCoord).b;
    float fresnel = pow(1.0 - max(dot(normalWs, viewDirWs), 0.0), 2.0);
    vec3 finalColor = mix(sceneColor, reflectionColor, fresnel * 0.5 * reflective);

    o_Color = vec4(finalColor, 1.0);
}
