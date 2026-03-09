$input a_position, a_normal, a_texcoord0, a_texcoord1, a_indices, a_weight
$output v_position, v_normal, v_texcoord0

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

	mat4 skin = mat4(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
	for (int i = 0; i < 4; ++i) {
		int idx = int(a_indices[i]);
		float w = a_weight[i];
		if (w > 0.0 && idx >= 0 && idx < MAX_BONES) {
			skin += u_Bones[idx] * w;
		}
	}
	pos = mul(skin, pos);
	nrm = mul((mat3)skin, nrm);

	vec4 worldPos = mul(u_Model, pos);
	v_position = worldPos.xyz;
	mat3 normalMatrix = mat3(u_NormalMatrix);
	v_normal = mul(normalMatrix, nrm);

	float texCoordIndex = u_TexCoordIndex;
	vec2 baseUv = mix(a_texcoord0, a_texcoord1, step(0.5, texCoordIndex));
	vec2 uv = baseUv * u_UVTransform.zw;
	float c = cos(u_UVRotation);
	float s = sin(u_UVRotation);
	uv = mat2(c, -s, s, c) * uv;
	uv += u_UVTransform.xy;
	if (u_FlipV > 0.5) uv.y = 1.0 - uv.y;
	v_texcoord0 = uv;

	gl_Position = mul(u_Projection, mul(u_View, worldPos));
}
