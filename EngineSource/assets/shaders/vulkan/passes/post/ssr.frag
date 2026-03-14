#version 460

layout(location = 0) in vec2 v_TexCoord;
layout(location = 0) out vec4 o_Color;

layout(binding = 0) uniform sampler2D g_Position;
layout(binding = 1) uniform sampler2D g_Normal;
layout(binding = 2) uniform sampler2D g_Albedo;
layout(binding = 3) uniform sampler2D g_PBRParams;
layout(binding = 4) uniform sampler2D g_Depth;
layout(binding = 5) uniform sampler2D u_SceneTexture;
layout(binding = 6) uniform samplerCube u_Skybox;

layout(push_constant, std430) uniform SSRPush {
    mat4 u_ViewMatrix;
    mat4 u_ProjectionMatrix;
    mat4 u_InverseViewMatrix;
    vec4 u_ViewPos_NearFar; // xyz = view pos, w = near
    vec2 u_Resolution;
    float u_StepSize;
    float u_Thickness;
    float u_FarPlane;
    int u_MaxSteps;
} pc;

float linearizeDepth(float depthSample) {
    float zNdc = depthSample * 2.0 - 1.0;
    float nearPlane = pc.u_ViewPos_NearFar.w;
    float linear = (2.0 * nearPlane * pc.u_FarPlane) / (pc.u_FarPlane + nearPlane - zNdc * (pc.u_FarPlane - nearPlane));
    return -linear;
}

vec3 RayMarch(vec3 rayOriginVs, vec3 rayDirVs) {
    float stepSize = pc.u_StepSize;
    for (int i = 0; i < pc.u_MaxSteps; ++i) {
        rayOriginVs += rayDirVs * stepSize;
        vec4 clipPos = pc.u_ProjectionMatrix * vec4(rayOriginVs, 1.0);
        if (clipPos.w <= 0.0) break;
        vec2 screenCoord = clipPos.xy / clipPos.w * 0.5 + 0.5;
        if (screenCoord.x < 0.0 || screenCoord.x > 1.0 || screenCoord.y < 0.0 || screenCoord.y > 1.0) break;
        float sceneViewZ = linearizeDepth(texture(g_Depth, screenCoord).r);
        if (texture(g_Depth, screenCoord).r >= 0.9999) continue;
        if (abs(rayOriginVs.z - sceneViewZ) < pc.u_Thickness)
            return texture(u_SceneTexture, screenCoord).rgb;
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
    vec3 u_ViewPos = pc.u_ViewPos_NearFar.xyz;
    vec3 viewDirWs = normalize(u_ViewPos - positionWs);
    vec3 reflectDirWs = reflect(-viewDirWs, normalWs);
    vec3 rayOriginVs = (pc.u_ViewMatrix * vec4(positionWs, 1.0)).xyz;
    vec3 rayDirVs = normalize((pc.u_ViewMatrix * vec4(reflectDirWs, 0.0)).xyz);
    vec3 reflectionColor = RayMarch(rayOriginVs, rayDirVs);
    if (reflectionColor.r < -0.5)
        reflectionColor = texture(u_Skybox, reflectDirWs).rgb;
    if (reflectionColor.r < -0.5) reflectionColor = sceneColor;
    float roughness = texture(g_PBRParams, v_TexCoord).g;
    reflectionColor = mix(reflectionColor, texture(u_Skybox, reflectDirWs).rgb, roughness);
    float edgeFade = smoothstep(0.0, 0.1, v_TexCoord.x) * smoothstep(0.0, 0.1, v_TexCoord.y)
                   * smoothstep(0.0, 0.1, 1.0 - v_TexCoord.x) * smoothstep(0.0, 0.1, 1.0 - v_TexCoord.y);
    reflectionColor *= edgeFade;
    vec3 albedo = texture(g_Albedo, v_TexCoord).rgb;
    float metallic = texture(g_PBRParams, v_TexCoord).r;
    float NdotV = max(dot(normalWs, viewDirWs), 0.0);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = F0 + (1.0 - F0) * pow(1.0 - NdotV, 5.0);
    float reflective = texture(g_PBRParams, v_TexCoord).b;
    o_Color = vec4(mix(sceneColor, F * reflectionColor, reflective), 1.0);
}
