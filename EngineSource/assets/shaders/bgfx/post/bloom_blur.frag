$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(u_Texture, 0);

uniform vec4 u_BlurParams; // xy = resolution, z = horizontal (0/1), w = intensity

void main()
{
	vec2 texelSize = 1.0 / u_BlurParams.xy;
	vec3 result = vec3(0.0);
	float weights[5] = { 0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216 };
	result += texture2D(u_Texture, v_texcoord0).rgb * weights[0];
	float blurRadius = u_BlurParams.w;
	bool horizontal = u_BlurParams.z > 0.5;
	if (horizontal) {
		for (int i = 1; i < 5; i++) {
			float offset = float(i) * blurRadius;
			result += texture2D(u_Texture, v_texcoord0 + vec2(texelSize.x * offset, 0.0)).rgb * weights[i];
			result += texture2D(u_Texture, v_texcoord0 - vec2(texelSize.x * offset, 0.0)).rgb * weights[i];
		}
	} else {
		for (int i = 1; i < 5; i++) {
			float offset = float(i) * blurRadius;
			result += texture2D(u_Texture, v_texcoord0 + vec2(0.0, texelSize.y * offset)).rgb * weights[i];
			result += texture2D(u_Texture, v_texcoord0 - vec2(0.0, texelSize.y * offset)).rgb * weights[i];
		}
	}
	gl_FragColor = vec4(result, 1.0);
}
