$input a_position, a_normal, a_texcoord0, a_tangent
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

void main()
{
	vec4 worldPos = mul(u_Model, vec4(a_position, 1.0));
	v_worldPos = worldPos.xyz;
	vec4 viewPos = mul(u_View, worldPos);
	v_viewPos = viewPos.xyz;

	mat3 normalMatrix = mat3(u_NormalMatrix);
	vec3 worldNormal = normalize(mul(normalMatrix, a_normal));
	vec3 worldTangent = a_tangent;
	if (length(worldTangent) < 0.001) {
		vec3 up = abs(worldNormal.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
		worldTangent = normalize(cross(up, worldNormal));
	} else {
		worldTangent = normalize(mul(normalMatrix, worldTangent));
	}
	vec3 worldBitangent = normalize(cross(worldNormal, worldTangent));
	v_worldNormal = worldNormal;
	v_worldTangent = worldTangent;
	v_worldBitangent = worldBitangent;

	vec2 uv = a_texcoord0 * u_UVTransform.zw;
	float c = cos(u_UVRotation);
	float s = sin(u_UVRotation);
	uv = mat2(c, -s, s, c) * uv;
	uv += u_UVTransform.xy;
	if (u_FlipV > 0.5) uv.y = 1.0 - uv.y;
	v_texcoord0 = uv;

	gl_Position = mul(u_Projection, viewPos);
}
