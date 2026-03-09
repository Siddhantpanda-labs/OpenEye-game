$input v_texcoord0

#include <bgfx_shader.sh>

SAMPLER2D(u_Texture, 0);

uniform vec2 u_Resolution;

#define FXAA_REDUCE_MIN (1.0 / 128.0)
#define FXAA_REDUCE_MUL (1.0 / 8.0)
#define FXAA_SPAN_MAX (8.0)

void main()
{
	vec2 texelSize = 1.0 / u_Resolution;
	vec2 nw = v_texcoord0 + vec2(-1.0, -1.0) * texelSize;
	vec2 ne = v_texcoord0 + vec2( 1.0, -1.0) * texelSize;
	vec2 sw = v_texcoord0 + vec2(-1.0,  1.0) * texelSize;
	vec2 se = v_texcoord0 + vec2( 1.0,  1.0) * texelSize;
	vec3 rgbNW = texture2D(u_Texture, nw).rgb;
	vec3 rgbNE = texture2D(u_Texture, ne).rgb;
	vec3 rgbSW = texture2D(u_Texture, sw).rgb;
	vec3 rgbSE = texture2D(u_Texture, se).rgb;
	vec3 rgbM = texture2D(u_Texture, v_texcoord0).rgb;

	vec3 luma = vec3(0.299, 0.587, 0.114);
	float lumaNW = dot(rgbNW, luma);
	float lumaNE = dot(rgbNE, luma);
	float lumaSW = dot(rgbSW, luma);
	float lumaSE = dot(rgbSE, luma);
	float lumaM = dot(rgbM, luma);

	float lumaMin = min(lumaM, min(min(lumaNW, lumaNE), min(lumaSW, lumaSE)));
	float lumaMax = max(lumaM, max(max(lumaNW, lumaNE), max(lumaSW, lumaSE)));

	vec2 dir;
	dir.x = -((lumaNW + lumaNE) - (lumaSW + lumaSE));
	dir.y = ((lumaNW + lumaSW) - (lumaNE + lumaSE));

	float dirReduce = max((lumaNW + lumaNE + lumaSW + lumaSE) * (0.25 * FXAA_REDUCE_MUL), FXAA_REDUCE_MIN);
	float rcpDirMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + dirReduce);
	dir = min(vec2(FXAA_SPAN_MAX, FXAA_SPAN_MAX), max(vec2(-FXAA_SPAN_MAX, -FXAA_SPAN_MAX), dir * rcpDirMin)) * texelSize;

	vec3 rgbA = 0.5 * (
		texture2D(u_Texture, v_texcoord0 + dir * (1.0/3.0 - 0.5)).rgb +
		texture2D(u_Texture, v_texcoord0 + dir * (2.0/3.0 - 0.5)).rgb);
	vec3 rgbB = rgbA * 0.5 + 0.25 * (
		texture2D(u_Texture, v_texcoord0 + dir * -0.5).rgb +
		texture2D(u_Texture, v_texcoord0 + dir * 0.5).rgb);

	float lumaB = dot(rgbB, luma);
	if ((lumaB < lumaMin) || (lumaB > lumaMax))
		gl_FragColor = vec4(rgbA, 1.0);
	else
		gl_FragColor = vec4(rgbB, 1.0);
}
