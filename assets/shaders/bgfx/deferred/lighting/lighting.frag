$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(u_Position, 0);
SAMPLER2D(u_Normal, 1);
SAMPLER2D(u_Albedo, 2);
SAMPLER2D(u_PBR, 3);
SAMPLER2D(u_ShadowMap, 5);

uniform vec3 u_ViewPos;
uniform vec3 u_LightDir;
uniform vec3 u_LightColor;
uniform float u_LightIntensity;
uniform vec3 u_Ambient;
uniform float u_Exposure;
uniform mat4 u_LightSpaceMatrix;
uniform float u_ShadowBias;
uniform int u_UseShadows;

#define MAX_POINT_LIGHTS 8
#define MAX_SPOT_LIGHTS 4
uniform int u_PointLightCount;
uniform vec3 u_PointLightPos[MAX_POINT_LIGHTS];
uniform vec3 u_PointLightColor[MAX_POINT_LIGHTS];
uniform float u_PointLightIntensity[MAX_POINT_LIGHTS];
uniform float u_PointLightRadius[MAX_POINT_LIGHTS];

uniform int u_SpotLightCount;
uniform vec3 u_SpotLightPos[MAX_SPOT_LIGHTS];
uniform vec3 u_SpotLightDir[MAX_SPOT_LIGHTS];
uniform vec3 u_SpotLightColor[MAX_SPOT_LIGHTS];
uniform float u_SpotLightIntensity[MAX_SPOT_LIGHTS];
uniform float u_SpotLightRange[MAX_SPOT_LIGHTS];
uniform float u_SpotLightInnerCos[MAX_SPOT_LIGHTS];
uniform float u_SpotLightOuterCos[MAX_SPOT_LIGHTS];

float shadowFactor(vec3 worldPos)
{
	if (u_UseShadows <= 0) return 0.0;
	vec4 lightSpacePos = mul(u_LightSpaceMatrix, vec4(worldPos, 1.0));
	vec3 ndc = lightSpacePos.xyz / lightSpacePos.w;
	ndc = ndc * 0.5 + 0.5;
	if (ndc.z > 1.0 || ndc.x < 0.0 || ndc.x > 1.0 || ndc.y < 0.0 || ndc.y > 1.0)
		return 0.0;
	float depth = texture2D(u_ShadowMap, ndc.xy).r;
	return (ndc.z - u_ShadowBias > depth) ? 1.0 : 0.0;
}

vec3 evalPointLight(vec3 pos, vec3 N, vec3 V, vec3 albedo, float metallic, float roughness,
	vec3 lightPos, vec3 lightColor, float intensity, float radius)
{
	vec3 L = lightPos - pos;
	float dist = length(L);
	if (dist >= radius) return vec3(0.0);
	L /= dist;
	float att = 1.0 - smoothstep(radius * 0.5, radius, dist);
	att *= att;
	float NdotL = max(dot(N, L), 0.0);
	vec3 H = normalize(V + L);
	float NdotH = max(dot(N, H), 0.0);
	float spec = pow(NdotH, 64.0 * (1.0 - roughness));
	vec3 diff = albedo * (1.0 - metallic) * lightColor * intensity * NdotL * att;
	diff += (0.04 + albedo * metallic) * lightColor * intensity * spec * att;
	return diff;
}

vec3 evalSpotLight(vec3 pos, vec3 N, vec3 V, vec3 albedo, float metallic, float roughness,
	vec3 lightPos, vec3 lightDir, vec3 lightColor, float intensity, float range,
	float innerCos, float outerCos)
{
	vec3 L = lightPos - pos;
	float dist = length(L);
	if (dist >= range) return vec3(0.0);
	L /= dist;
	float cosTheta = dot(-L, lightDir);
	float spot = smoothstep(outerCos, innerCos, cosTheta);
	float att = (1.0 - dist / range) * (1.0 - dist / range);
	att *= spot;
	float NdotL = max(dot(N, L), 0.0);
	vec3 H = normalize(V + L);
	float NdotH = max(dot(N, H), 0.0);
	float spec = pow(NdotH, 64.0 * (1.0 - roughness));
	vec3 diff = albedo * (1.0 - metallic) * lightColor * intensity * NdotL * att;
	diff += (0.04 + albedo * metallic) * lightColor * intensity * spec * att;
	return diff;
}

void main()
{
	vec3 pos = texture2D(u_Position, v_texcoord0).rgb;
	vec3 N = normalize(texture2D(u_Normal, v_texcoord0).rgb);
	vec3 albedo = texture2D(u_Albedo, v_texcoord0).rgb;
	vec2 mr = texture2D(u_PBR, v_texcoord0).rg;
	float metallic = mr.r;
	float roughness = max(mr.g, 0.04);
	roughness = max(roughness * 0.8, 0.04);

	vec3 V = normalize(u_ViewPos - pos);
	vec3 L = normalize(-u_LightDir);
	float NdotL = max(dot(N, L), 0.0);

	float shadow = shadowFactor(pos);
	float lightVis = 1.0 - shadow;

	vec3 diff = albedo * (1.0 - metallic) * u_Ambient;
	diff += albedo * u_LightColor * u_LightIntensity * NdotL * lightVis;

	vec3 H = normalize(V + L);
	float NdotH = max(dot(N, H), 0.0);
	float spec = pow(NdotH, 64.0 * (1.0 - roughness));
	diff += (0.04 + albedo * metallic) * u_LightColor * u_LightIntensity * spec * lightVis;

	int numPoint = min(u_PointLightCount, MAX_POINT_LIGHTS);
	int numSpot = min(u_SpotLightCount, MAX_SPOT_LIGHTS);
	for (int i = 0; i < numPoint; i++) {
		diff += evalPointLight(pos, N, V, albedo, metallic, roughness,
			u_PointLightPos[i], u_PointLightColor[i], u_PointLightIntensity[i], u_PointLightRadius[i]);
	}
	for (int i = 0; i < numSpot; i++) {
		diff += evalSpotLight(pos, N, V, albedo, metallic, roughness,
			u_SpotLightPos[i], normalize(u_SpotLightDir[i]), u_SpotLightColor[i],
			u_SpotLightIntensity[i], u_SpotLightRange[i],
			u_SpotLightInnerCos[i], u_SpotLightOuterCos[i]);
	}

	vec3 color = diff * u_Exposure;
	float lum = dot(color, vec3(0.2126, 0.7152, 0.0722));
	color = mix(vec3(lum), color, 1.15);
	color = clamp(color, vec3(0.0), vec3(10.0));

	gl_FragColor = vec4(color, 1.0);
}
