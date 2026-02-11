$input v_worldPos, v_viewPos, v_worldNormal, v_texcoord0, v_worldTangent, v_worldBitangent

#include <bgfx_shader.sh>

SAMPLER2D(u_AlbedoTexture, 0);
SAMPLER2D(u_NormalTexture, 1);
SAMPLER2D(u_MetallicRoughnessTexture, 2);
SAMPLER2D(u_AOTexture, 3);
SAMPLER2D(u_EmissiveTexture, 4);
SAMPLER2D(u_ShadowMap, 5);

uniform vec3 u_ViewPos;
uniform vec3 u_Albedo;
uniform float u_Metallic;
uniform float u_Roughness;
uniform float u_AO;
uniform vec3 u_Emission;
uniform float u_Alpha;
uniform vec3 u_DirLightDir;
uniform vec3 u_DirLightColor;
uniform float u_DirLightIntensity;
uniform vec3 u_Ambient;
uniform float u_Exposure;
uniform mat4 u_LightSpaceMatrix;
uniform float u_ShadowBias;
uniform int u_UseShadows;
uniform int u_UseAlbedoTexture;
uniform int u_UseNormalTexture;
uniform int u_UseMetallicRoughnessTexture;
uniform int u_UseAOTexture;
uniform int u_UseEmissiveTexture;

#define MAX_POINT_LIGHTS 8
#define MAX_SPOT_LIGHTS 4
uniform int u_PointLightCount;
uniform int u_SpotLightCount;
uniform vec3 u_PointLightPos[MAX_POINT_LIGHTS];
uniform vec3 u_PointLightColor[MAX_POINT_LIGHTS];
uniform float u_PointLightIntensity[MAX_POINT_LIGHTS];
uniform float u_PointLightRadius[MAX_POINT_LIGHTS];
uniform vec3 u_SpotLightPos[MAX_SPOT_LIGHTS];
uniform vec3 u_SpotLightDir[MAX_SPOT_LIGHTS];
uniform vec3 u_SpotLightColor[MAX_SPOT_LIGHTS];
uniform float u_SpotLightIntensity[MAX_SPOT_LIGHTS];
uniform float u_SpotLightRange[MAX_SPOT_LIGHTS];
uniform float u_SpotLightInnerCos[MAX_SPOT_LIGHTS];
uniform float u_SpotLightOuterCos[MAX_SPOT_LIGHTS];

float shadowFactor(vec3 worldPos) {
	if (u_UseShadows <= 0) return 0.0;
	vec4 lightSpacePos = mul(u_LightSpaceMatrix, vec4(worldPos, 1.0));
	vec3 ndc = lightSpacePos.xyz / lightSpacePos.w;
	ndc = ndc * 0.5 + 0.5;
	if (ndc.z > 1.0 || ndc.x < 0.0 || ndc.x > 1.0 || ndc.y < 0.0 || ndc.y > 1.0)
		return 0.0;
	float depth = texture2D(u_ShadowMap, ndc.xy).r;
	return (ndc.z - u_ShadowBias > depth) ? 1.0 : 0.0;
}

vec3 calcPointLight(vec3 pos, vec3 color, float intensity, float radius,
	vec3 fragPos, vec3 normal, vec3 viewDir, vec3 albedo, float metallic, float roughness) {
	vec3 lightDir = normalize(pos - fragPos);
	float dist = length(pos - fragPos);
	float atten = 1.0 / (1.0 + 0.09 * dist + 0.032 * dist * dist);
	atten *= smoothstep(radius, radius * 0.5, dist);
	float NdotL = max(dot(normal, lightDir), 0.0);
	vec3 H = normalize(viewDir + lightDir);
	float NdotH = max(dot(normal, H), 0.0);
	float spec = pow(NdotH, 64.0 * (1.0 - roughness));
	vec3 diff = albedo * (1.0 - metallic) * NdotL + (0.04 + albedo * metallic) * spec;
	return diff * color * intensity * atten;
}

vec3 calcSpotLight(vec3 pos, vec3 dir, vec3 color, float intensity, float range,
	float innerCos, float outerCos, vec3 fragPos, vec3 normal, vec3 viewDir,
	vec3 albedo, float metallic, float roughness) {
	vec3 lightDir = normalize(pos - fragPos);
	float dist = length(pos - fragPos);
	if (dist > range) return vec3(0.0);
	float cosAngle = dot(lightDir, -dir);
	float spot = smoothstep(outerCos, innerCos, cosAngle);
	float atten = 1.0 / (1.0 + 0.09 * dist + 0.032 * dist * dist);
	float NdotL = max(dot(normal, lightDir), 0.0);
	vec3 H = normalize(viewDir + lightDir);
	float NdotH = max(dot(normal, H), 0.0);
	float spec = pow(NdotH, 64.0 * (1.0 - roughness));
	vec3 diff = albedo * (1.0 - metallic) * NdotL + (0.04 + albedo * metallic) * spec;
	return diff * color * intensity * atten * spot;
}

void main() {
	vec3 N = normalize(v_worldNormal);
	if (u_UseNormalTexture > 0) {
		mat3 TBN = mat3(normalize(v_worldTangent), normalize(v_worldBitangent), normalize(v_worldNormal));
		vec3 n = texture2D(u_NormalTexture, v_texcoord0).xyz * 2.0 - 1.0;
		N = normalize(mul(TBN, n));
	}
	vec3 V = normalize(u_ViewPos - v_worldPos);
	vec3 albedo = u_UseAlbedoTexture > 0
		? texture2D(u_AlbedoTexture, v_texcoord0).rgb * u_Albedo
		: u_Albedo;
	float metallic = u_Metallic;
	float roughness = u_Roughness;
	if (u_UseMetallicRoughnessTexture > 0) {
		vec2 mr = texture2D(u_MetallicRoughnessTexture, v_texcoord0).bg;
		metallic = mr.x;
		roughness = mr.y;
	}
	roughness = max(roughness, 0.04);

	float ao = u_AO;
	if (u_UseAOTexture > 0) ao = texture2D(u_AOTexture, v_texcoord0).r;

	vec3 emission = u_Emission;
	if (u_UseEmissiveTexture > 0) emission = texture2D(u_EmissiveTexture, v_texcoord0).rgb;

	vec3 result = albedo * u_Ambient * ao;
	vec3 L = normalize(-u_DirLightDir);
	float NdotL = max(dot(N, L), 0.0);
	float shadow = shadowFactor(v_worldPos);
	float lightVis = 1.0 - shadow;
	result += albedo * u_DirLightColor * u_DirLightIntensity * NdotL * lightVis;

	int numPoint = min(u_PointLightCount, MAX_POINT_LIGHTS);
	int numSpot = min(u_SpotLightCount, MAX_SPOT_LIGHTS);
	for (int i = 0; i < numPoint; i++) {
		result += calcPointLight(
			u_PointLightPos[i], u_PointLightColor[i],
			u_PointLightIntensity[i], u_PointLightRadius[i],
			v_worldPos, N, V, albedo, metallic, roughness);
	}
	for (int i = 0; i < numSpot; i++) {
		result += calcSpotLight(
			u_SpotLightPos[i], normalize(u_SpotLightDir[i]),
			u_SpotLightColor[i], u_SpotLightIntensity[i], u_SpotLightRange[i],
			u_SpotLightInnerCos[i], u_SpotLightOuterCos[i],
			v_worldPos, N, V, albedo, metallic, roughness);
	}

	result += emission;
	gl_FragColor = vec4(result * u_Exposure, u_Alpha);
}
