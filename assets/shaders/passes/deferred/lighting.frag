#version 450 core

out vec4 FragColor;
in vec2 v_TexCoord;

uniform sampler2D g_Position;
uniform sampler2D g_Normal;
uniform sampler2D g_Albedo;
uniform sampler2D g_PBRParams;
uniform int u_DebugMode = 0;

#define MAX_CASCADES 4
uniform int u_ShadowCascadeCount = 4;
uniform sampler2D u_ShadowMaps[MAX_CASCADES];
uniform mat4 u_ShadowMatrices[MAX_CASCADES];
uniform sampler2D u_Background;

#include "../../common/camera.glsl"
#include "../../common/light.glsl"
#include "../../common/brdf.glsl"
#include "../../common/shadows.glsl"

// Point light uniforms (temporary until clustered lighting)
#define MAX_POINT_LIGHTS 32
uniform int u_PointLightCount;
uniform vec3 u_PointLightPositions[MAX_POINT_LIGHTS];
uniform vec3 u_PointLightColors[MAX_POINT_LIGHTS];
uniform float u_PointLightIntensities[MAX_POINT_LIGHTS];
uniform float u_PointLightRanges[MAX_POINT_LIGHTS];

// IBL uniforms
uniform samplerCube u_IrradianceMap;
uniform samplerCube u_PrefilterMap;
uniform sampler2D u_BRDFLUT;
uniform bool u_UseIBL = true;

void main(){
    vec3 position = texture(g_Position, v_TexCoord).rgb;
    vec3 normal = normalize(texture(g_Normal, v_TexCoord).rgb);
    vec3 albedo = texture(g_Albedo, v_TexCoord).rgb;
    vec2 pbr = texture(g_PBRParams, v_TexCoord).rg;
    float metallic=pbr.r; float roughness=max(pbr.g,0.04);
    
    // Show skybox where there's no geometry (check if position is near origin/invalid)
    // Position texture should have invalid positions at (0,0,0) where depth test failed
    if (length(position) < 0.001) {
        vec3 sky = texture(u_Background, v_TexCoord).rgb;
        FragColor = vec4(sky, 1.0);
        return;
    }
    vec3 V = normalize(u_ViewPos - position);
    if (u_DebugMode==1){ FragColor=vec4(albedo,1.0); return; }
    if (u_DebugMode==2){ FragColor=vec4(normal*0.5+0.5,1.0); return; }
    if (u_DebugMode==3){ FragColor=vec4(position*0.1,1.0); return; }
    if (u_DebugMode==4){ FragColor=vec4(metallic,roughness,0.0,1.0); return; }
    vec3 Ld = normalize(-u_LightDir); float NdotL = max(dot(normal,Ld),0.0);
    vec3 lighting=vec3(0.0);
    if (u_LightIntensity>0.0 && NdotL>0.0){ float vis=CalculateShadow(position,normal,Ld,u_ShadowMaps,u_ShadowMatrices,u_CascadeSplits,u_ViewMatrix); vec3 brdf=EvaluateBurleyBRDF(albedo,metallic,roughness,normal,V,Ld); lighting += brdf * (u_LightColor*u_LightIntensity) * NdotL * vis; }
    for (int i=0;i<u_PointLightCount && i<MAX_POINT_LIGHTS;i++){ vec3 lp=u_PointLightPositions[i]; vec3 L=normalize(lp-position); float d=length(lp-position); if (d>u_PointLightRanges[i]) continue; float att=1.0/(1.0+0.09*d+0.032*d*d); float rf=1.0 - smoothstep(0.0,u_PointLightRanges[i],d); float ndl=max(dot(normal,L),0.0); if (ndl>0.0){ vec3 brdf=EvaluateBurleyBRDF(albedo,metallic,roughness,normal,V,L); lighting += brdf * (u_PointLightColors[i]*u_PointLightIntensities[i]) * ndl * att * rf; } }
    
    // IBL (Image-Based Lighting)
    vec3 ambient = vec3(0.0);
    
    // Base ambient contribution (always applied)
    vec3 baseAmbient = albedo * u_AmbientLight;
    
    if (u_UseIBL) {
        vec3 F0 = mix(vec3(0.04), albedo, metallic);
        vec3 F = FresnelSchlickRoughness(max(dot(normal, V), 0.0), F0, roughness);
        vec3 kS = F;
        vec3 kD = (1.0 - kS) * (1.0 - metallic);
        vec3 irradiance = texture(u_IrradianceMap, normal).rgb;
        vec3 diffuse = irradiance * albedo;
        
        vec3 R = reflect(-V, normal);
        float maxMipLevel = 4.0;
        float mipLevel = roughness * maxMipLevel;
        vec3 prefilteredColor = textureLod(u_PrefilterMap, R, mipLevel).rgb;
        vec2 brdfSample = texture(u_BRDFLUT, vec2(max(dot(normal, V), 0.0), roughness)).rg;
        vec3 specular = prefilteredColor * (F * brdfSample.x + brdfSample.y);
        
        // IBL provides ambient, blend with base ambient for fallback
        vec3 iblAmbient = kD * diffuse + specular;
        // Blend IBL with base ambient (IBL dominant when present, base ambient as fallback)
        ambient = mix(baseAmbient, iblAmbient, 0.95) + baseAmbient * 0.05;
    } else {
        // No IBL - use base ambient only
        ambient = baseAmbient;
    }
    
    vec3 color = lighting + ambient;
    // Apply exposure (but don't tone map - that's done in post-processing)
    color *= max(u_Exposure, 0.0001);
    // Output HDR values (raw, no clamping)
    FragColor = vec4(color, 1.0);
}


