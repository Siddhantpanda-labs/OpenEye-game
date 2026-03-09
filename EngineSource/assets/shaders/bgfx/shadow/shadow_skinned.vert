$input a_position, a_indices, a_weight

#include <bgfx_shader.sh>

uniform mat4 u_Model;
uniform mat4 u_LightSpaceMatrix;

#define MAX_BONES 128
uniform mat4 u_Bones[MAX_BONES];

void main()
{
	vec4 pos = vec4(a_position, 1.0);
	mat4 skin = mat4(0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0);
	for (int i = 0; i < 4; ++i) {
		int idx = int(a_indices[i]);
		float w = a_weight[i];
		if (w > 0.0 && idx >= 0 && idx < MAX_BONES)
			skin += u_Bones[idx] * w;
	}
	pos = mul(skin, pos);
	vec4 worldPos = mul(u_Model, pos);
	gl_Position = mul(u_LightSpaceMatrix, worldPos);
}
