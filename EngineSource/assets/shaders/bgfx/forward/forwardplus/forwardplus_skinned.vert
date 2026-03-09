$input a_position, a_normal, a_texcoord0, a_texcoord1, a_tangent, a_indices, a_weight
$output v_worldPos, v_viewPos, v_worldNormal, v_texcoord0, v_worldTangent, v_worldBitangent

#include <bgfx_shader.sh>

uniform mat4 u_Model;
uniform mat4 u_View;
uniform mat4 u_Projection;
uniform mat4 u_NormalMatrix;
uniform vec4 u_UVTransform;
uniform float u_UVRotation;
uniform float u_TexCoordIndex;
uniform float u_FlipV;

#define MAX_BONES 128
uniform mat4 u_Bones[MAX_BONES];

void main()
{
	vec4 pos = vec4(a_position, 1.0);
	vec3 nrm = a_normal;
	vec3 tang = a_tangent;

	mat4 skin = mat4(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
	float totalWeight = 0.0;
	for (int i = 0; i < 4; ++i) {
		int idx = int(a_indices[i]);
		float w = a_weight[i];
		if (w > 0.0 && idx >= 0 && idx < MAX_BONES) {
			skin += u_Bones[idx] * w;
			totalWeight += w;
		}
	}
	if (totalWeight <= 0.0)
		skin = mat4(1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0);
	pos = mul(skin, pos);
	nrm = mul((mat3)skin, nrm);
	tang = mul((mat3)skin, tang);

	vec4 worldPos = mul(u_Model, pos);
	v_worldPos = worldPos.xyz;
	vec4 viewPos = mul(u_View, worldPos);
	v_viewPos = viewPos.xyz;

	mat3 normalMatrix = mat3(u_NormalMatrix);
	vec3 worldNormal = normalize(mul(normalMatrix, nrm));
	vec3 worldTangent;
	if (length(tang) < 0.001) {
		vec3 up = abs(worldNormal.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
		worldTangent = normalize(cross(up, worldNormal));
	} else {
		worldTangent = normalize(mul(normalMatrix, tang));
	}
	vec3 worldBitangent = normalize(cross(worldNormal, worldTangent));
	v_worldNormal = worldNormal;
	v_worldTangent = worldTangent;
	v_worldBitangent = worldBitangent;

	float texCoordIndex = u_TexCoordIndex;
	vec2 baseUv = mix(a_texcoord0, a_texcoord1, step(0.5, texCoordIndex));
	vec2 uv = baseUv * u_UVTransform.zw;
	float c = cos(u_UVRotation);
	float s = sin(u_UVRotation);
	uv = mat2(c, -s, s, c) * uv;
	uv += u_UVTransform.xy;
	if (u_FlipV > 0.5) uv.y = 1.0 - uv.y;
	v_texcoord0 = uv;

	gl_Position = mul(u_Projection, viewPos);
}
