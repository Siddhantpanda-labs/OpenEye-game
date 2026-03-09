$input a_position, a_normal, a_texcoord0
$output v_position, v_normal, v_texcoord0

#include <bgfx_shader.sh>

uniform mat4 u_Model;
uniform mat4 u_View;
uniform mat4 u_Projection;
uniform mat4 u_NormalMatrix; // upper 3x3 = transpose(inverse(mat3(u_Model))), set on CPU

void main()
{
	vec4 worldPos = mul(u_Model, vec4(a_position, 1.0));
	v_position = worldPos.xyz;
	mat3 normalMatrix = mat3(u_NormalMatrix);
	v_normal = normalize(mul(normalMatrix, a_normal));
	v_texcoord0 = vec2(a_texcoord0.x, 1.0 - a_texcoord0.y);
	gl_Position = mul(u_Projection, mul(u_View, worldPos));
}
