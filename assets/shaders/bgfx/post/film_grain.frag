$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(u_Texture, 0);

uniform float u_Intensity;
uniform float u_Time;

float random(vec2 co) {
	return fract(sin(dot(co.xy, vec2(12.9898, 78.233)) + u_Time) * 43758.5453);
}

void main()
{
	vec3 color = texture2D(u_Texture, v_texcoord0).rgb;
	float grain = random(v_texcoord0 * 1000.0) * 2.0 - 1.0;
	grain *= u_Intensity;
	color += grain * 0.1;
	gl_FragColor = vec4(color, 1.0);
}
