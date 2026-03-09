$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(u_Texture, 0);
SAMPLER2D(g_Velocity, 1);

uniform vec2 u_Resolution;
uniform float u_Intensity;
uniform int u_Samples;

void main()
{
	vec2 velocity = texture2D(g_Velocity, v_texcoord0).rg;
	if (length(velocity) < 0.001) {
		gl_FragColor = texture2D(u_Texture, v_texcoord0);
		return;
	}
	vec3 color = texture2D(u_Texture, v_texcoord0).rgb;
	vec2 texelSize = 1.0 / u_Resolution;
	vec2 offset = velocity * u_Intensity * texelSize;
	float numSamples = float(max(u_Samples, 1));
	float weight = 1.0 / (numSamples + 1.0);
	color *= weight;
	for (int i = 1; i <= int(numSamples); i++) {
		vec2 sampleCoord = v_texcoord0 + offset * float(i);
		color += texture2D(u_Texture, sampleCoord).rgb * weight;
	}
	gl_FragColor = vec4(color, 1.0);
}
