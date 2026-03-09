$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(u_Texture, 0);

uniform vec2 u_Resolution;
uniform float u_Intensity;

void main()
{
	vec2 center = vec2(0.5, 0.5);
	vec2 dist = v_texcoord0 - center;
	float distFromCenter = length(dist);
	vec2 offsetDir = normalize(dist);
	float offset = u_Intensity * distFromCenter;

	vec2 redOffset = v_texcoord0 + offsetDir * offset;
	float r = texture2D(u_Texture, redOffset).r;
	float g = texture2D(u_Texture, v_texcoord0).g;
	vec2 blueOffset = v_texcoord0 - offsetDir * offset * 0.5;
	float b = texture2D(u_Texture, blueOffset).b;

	gl_FragColor = vec4(r, g, b, 1.0);
}
