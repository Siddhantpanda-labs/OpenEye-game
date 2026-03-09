$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(u_Texture, 0);

uniform float u_Intensity;
uniform float u_Radius;
uniform float u_Softness;

void main()
{
	vec3 color = texture2D(u_Texture, v_texcoord0).rgb;
	vec2 center = vec2(0.5, 0.5);
	float dist = distance(v_texcoord0, center);
	float vignette = smoothstep(u_Radius, u_Radius + u_Softness, dist);
	vignette = 1.0 - (vignette * u_Intensity);
	color *= vignette;
	gl_FragColor = vec4(color, 1.0);
}
