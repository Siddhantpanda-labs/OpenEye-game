$input a_position, a_normal, a_texcoord0, a_texcoord1, a_tangent, a_indices, a_weight
$output v_worldPos, v_worldNormal, v_texcoord0, v_velocity, v_worldTangent

#include <bgfx_shader.sh>

uniform mat4 u_Model;
uniform mat4 u_NormalMatrix; // 3x3 = transpose(inverse(mat3(u_Model))), set on CPU
uniform mat4 u_View;
uniform mat4 u_Projection;
uniform mat4 u_PrevView;
uniform mat4 u_PrevProjection;
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
	mat3 normalMatrix = mat3(u_NormalMatrix);
	vec3 worldNormal = normalize(mul(normalMatrix, nrm));
	vec3 worldTangent;
	if (length(tang) < 0.001) {
		vec3 up = abs(worldNormal.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
		worldTangent = normalize(cross(up, worldNormal));
	} else {
		worldTangent = normalize(mul(normalMatrix, tang));
	}

	v_worldPos = worldPos.xyz;
	v_worldNormal = worldNormal;
	float uvRot = u_UVRotation;
	float texCoordIndex = u_TexCoordIndex;
	float flipV = u_FlipV;
	vec2 baseUv = mix(a_texcoord0, a_texcoord1, step(0.5, texCoordIndex));
	vec2 uv = baseUv * u_UVTransform.zw;
	float c = cos(uvRot);
	float s = sin(uvRot);
	uv = mat2(c, -s, s, c) * uv;
	uv += u_UVTransform.xy;
	if (flipV > 0.5) uv.y = 1.0 - uv.y;
	v_texcoord0 = uv;

	vec4 currentPos = mul(u_Projection, mul(u_View, worldPos));
	vec4 prevPos = mul(u_PrevProjection, mul(u_PrevView, worldPos));
	vec2 currentNDC = currentPos.xy / currentPos.w;
	vec2 prevNDC = prevPos.xy / prevPos.w;
	v_velocity = currentNDC - prevNDC;

	v_worldTangent = worldTangent;

	gl_Position = currentPos;
}
