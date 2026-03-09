$input a_position

#include <bgfx_shader.sh>

uniform mat4 u_Model;
uniform mat4 u_LightSpaceMatrix;

void main()
{
	vec4 worldPos = mul(u_Model, vec4(a_position, 1.0));
	gl_Position = mul(u_LightSpaceMatrix, worldPos);
}
