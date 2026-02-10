$input v_position, v_normal, v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(u_ShadowMap, 0);

uniform vec4 u_Albedo;
uniform vec4 u_MaterialParams; // x=metallic, y=roughness, z=alpha
uniform vec3 u_Emission;
uniform vec3 u_LightDir;
uniform vec3 u_LightColor;
uniform vec3 u_ViewPos;
uniform vec4 u_AmbientExposure; // rgb=ambient, a=exposure
uniform vec4 u_LightIntensityAlpha; // x=lightIntensity, y=alpha
uniform mat4 u_LightSpaceMatrix;
uniform vec4 u_ShadowParams; // x=bias, y=useShadows (0/1)

#define MAX_POINT_LIGHTS 8
#define MAX_SPOT_LIGHTS 4
uniform vec4 u_PointLightCount;
uniform vec4 u_PointLightPos[MAX_POINT_LIGHTS];
uniform vec4 u_PointLightColor[MAX_POINT_LIGHTS];
uniform vec4 u_PointLightIntensityRadius[MAX_POINT_LIGHTS]; // x=intensity, y=radius
uniform vec4 u_SpotLightCount;
uniform vec4 u_SpotLightPos[MAX_SPOT_LIGHTS];
uniform vec4 u_SpotLightDir[MAX_SPOT_LIGHTS];
uniform vec4 u_SpotLightColor[MAX_SPOT_LIGHTS];
uniform vec4 u_SpotLightParams[MAX_SPOT_LIGHTS]; // x=intensity, y=range, z=innerCos, w=outerCos

float shadowFactor(vec3 worldPos) {
	if (u_ShadowParams.y <= 0.0) return 0.0;
	vec4 lightSpacePos = mul(u_LightSpaceMatrix, vec4(worldPos, 1.0));
	vec3 ndc = lightSpacePos.xyz / lightSpacePos.w;
	ndc = ndc * 0.5 + 0.5;
	if (ndc.z > 1.0 || ndc.x < 0.0 || ndc.x > 1.0 || ndc.y < 0.0 || ndc.y > 1.0)
		return 0.0;
	float depth = texture2D(u_ShadowMap, ndc.xy).r;
	return (ndc.z - u_ShadowParams.x > depth) ? 1.0 : 0.0;
}

vec3 evalPointLight(vec3 pos, vec3 N, vec3 V, vec3 albedo, float metallic, float roughness,
	vec3 lightPos, vec3 lightColor, float intensity, float radius) {
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
	float innerCos, float outerCos) {
	vec3 L = lightPos - pos;
	float dist = length(L);
	if (dist >= range) return vec3(0.0);
	L /= dist;
	float cosTheta = dot(-L, lightDir);
	float spot = smoothstep(outerCos, innerCos, cosTheta);
	float att = (1.0 - dist / range) * (1.0 - dist / range) * spot;
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
	vec3 N = normalize(v_normal);
	vec3 V = normalize(u_ViewPos - v_position);
	vec3 L = normalize(-u_LightDir);
	float NdotL = max(dot(N, L), 0.0);
	float shadow = shadowFactor(v_position);
	float lightVis = 1.0 - shadow;
	vec3 albedo = u_Albedo.rgb;
	float metallic = u_MaterialParams.x;
	float roughness = max(u_MaterialParams.y, 0.04);
	float alpha = u_MaterialParams.z;

	vec3 diffuse = albedo * u_LightColor * u_LightIntensityAlpha.x * NdotL * lightVis;
	vec3 ambient = albedo * u_AmbientExposure.rgb;
	vec3 color = ambient + diffuse + u_Emission;

	int numPoint = int(u_PointLightCount.x);
	int numSpot = int(u_SpotLightCount.x);
	for (int i = 0; i < MAX_POINT_LIGHTS; i++) {
		if (i >= numPoint) break;
		color += evalPointLight(v_position, N, V, albedo, metallic, roughness,
			u_PointLightPos[i].xyz, u_PointLightColor[i].rgb,
			u_PointLightIntensityRadius[i].x, u_PointLightIntensityRadius[i].y);
	}
	for (int i = 0; i < MAX_SPOT_LIGHTS; i++) {
		if (i >= numSpot) break;
		color += evalSpotLight(v_position, N, V, albedo, metallic, roughness,
			u_SpotLightPos[i].xyz, normalize(u_SpotLightDir[i].xyz), u_SpotLightColor[i].rgb,
			u_SpotLightParams[i].x, u_SpotLightParams[i].y,
			u_SpotLightParams[i].z, u_SpotLightParams[i].w);
	}

	color *= u_AmbientExposure.a;
	gl_FragColor = vec4(color, alpha);
}
