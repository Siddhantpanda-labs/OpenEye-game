$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(u_HDRTexture, 0);

uniform vec4 u_BloomExtractParams; // x = threshold

void main()
{
	vec3 color = texture2D(u_HDRTexture, v_texcoord0).rgb;
	float luminance = dot(color, vec3(0.2126, 0.7152, 0.0722));
	vec3 bloom = max(color - vec3(u_BloomExtractParams.x), vec3(0.0));
	float smoothFactor = smoothstep(u_BloomExtractParams.x, u_BloomExtractParams.x * 1.5, luminance);
	bloom *= smoothFactor;
	gl_FragColor = vec4(bloom, 1.0);
}
