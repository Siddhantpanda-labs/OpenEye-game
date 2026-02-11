$input v_worldPos, v_worldNormal, v_texcoord0, v_velocity, v_worldTangent

#include <bgfx_shader.sh>

SAMPLER2D(u_AlbedoTexture, 0);
SAMPLER2D(u_NormalTexture, 1);
SAMPLER2D(u_MetallicTexture, 2);
SAMPLER2D(u_RoughnessTexture, 3);
SAMPLER2D(u_AOTexture, 4);
SAMPLER2D(u_MetallicRoughnessTexture, 5);

uniform vec3 u_Albedo;
uniform float u_Metallic;
uniform float u_Roughness;
uniform float u_AO;
uniform float u_HasAlbedoTexture;
uniform float u_HasNormalTexture;
uniform float u_HasMetallicTexture;
uniform float u_HasRoughnessTexture;
uniform float u_HasAOTexture;
uniform float u_HasMetallicRoughnessTexture;

void main()
{
	vec4 g_Position = vec4(v_worldPos, 1.0);

	vec3 normal = normalize(v_worldNormal);
	if (u_HasNormalTexture > 0.5) {
		vec3 T = normalize(v_worldTangent - dot(v_worldTangent, normal) * normal);
		vec3 B = cross(normal, T);
		mat3 TBN = mat3(T, B, normal);
		vec3 normalSample = texture2D(u_NormalTexture, v_texcoord0).rgb;
		normalSample = normalSample * 2.0 - 1.0;
		normal = normalize(mul(TBN, normalSample));
	}
	if (isnan(normal.x) || isnan(normal.y) || isnan(normal.z)) normal = vec3(0.0, 1.0, 0.0);
	vec4 g_Normal = vec4(normal, 1.0);

	vec3 albedoColor = u_Albedo;
	if (u_HasAlbedoTexture > 0.5)
		albedoColor = texture2D(u_AlbedoTexture, v_texcoord0).rgb * u_Albedo;
	vec4 g_Albedo = vec4(clamp(albedoColor, vec3(0.0), vec3(1.0)), 1.0);

	float metallic = u_Metallic;
	float roughness = u_Roughness;
	if (u_HasMetallicRoughnessTexture > 0.5) {
		vec2 mr = texture2D(u_MetallicRoughnessTexture, v_texcoord0).bg;
		metallic = mr.x;
		roughness = mr.y;
	} else {
		if (u_HasMetallicTexture > 0.5) metallic = texture2D(u_MetallicTexture, v_texcoord0).r;
		if (u_HasRoughnessTexture > 0.5) roughness = texture2D(u_RoughnessTexture, v_texcoord0).r;
	}
	// u_AO from uniform; optional u_AOTexture sampled for AO could be applied here

	vec2 g_PBRParams = vec2(clamp(metallic, 0.0, 1.0), clamp(roughness, 0.04, 1.0));
	vec2 g_Velocity = v_velocity;

	gl_FragData[0] = g_Position;
	gl_FragData[1] = g_Normal;
	gl_FragData[2] = g_Albedo;
	gl_FragData[3] = vec4(g_PBRParams, 0.0, 0.0);
	gl_FragData[4] = vec4(g_Velocity, 0.0, 0.0);
}
