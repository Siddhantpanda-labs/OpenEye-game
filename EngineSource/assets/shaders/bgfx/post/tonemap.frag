$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(u_HDRTexture, 0);

uniform vec4 u_TonemapParams; // x = exposure, y = useACES (0/1)

vec3 ACESFilmic(vec3 x) {
	float a = 2.51;
	float b = 0.03;
	float c = 2.43;
	float d = 0.59;
	float e = 0.14;
	return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

vec3 Reinhard(vec3 color) {
	return color / (color + vec3(1.0));
}

void main()
{
	vec3 hdrColor = texture2D(u_HDRTexture, v_texcoord0).rgb;
	hdrColor *= u_TonemapParams.x;
	vec3 ldrColor;
	if (u_TonemapParams.y > 0.5)
		ldrColor = ACESFilmic(hdrColor);
	else
		ldrColor = Reinhard(hdrColor);
	gl_FragColor = vec4(ldrColor, 1.0);
}
