#version 460

layout(location = 0) in vec2 v_TexCoord;
layout(location = 0) out vec4 FragColor;

layout(binding = 0) uniform sampler2D u_Texture;

layout(push_constant, std430) uniform FXAAPush {
    vec2 u_Resolution;
} pc;

#define FXAA_REDUCE_MIN (1.0 / 128.0)
#define FXAA_REDUCE_MUL (1.0 / 8.0)
#define FXAA_SPAN_MAX (8.0)

void main() {
    vec2 texelSize = 1.0 / pc.u_Resolution;
    vec3 rgbNW = textureOffset(u_Texture, v_TexCoord, ivec2(-1, -1)).rgb;
    vec3 rgbNE = textureOffset(u_Texture, v_TexCoord, ivec2(1, -1)).rgb;
    vec3 rgbSW = textureOffset(u_Texture, v_TexCoord, ivec2(-1, 1)).rgb;
    vec3 rgbSE = textureOffset(u_Texture, v_TexCoord, ivec2(1, 1)).rgb;
    vec3 rgbM = texture(u_Texture, v_TexCoord).rgb;
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
        texture(u_Texture, v_TexCoord + dir * (1.0 / 3.0 - 0.5)).rgb +
        texture(u_Texture, v_TexCoord + dir * (2.0 / 3.0 - 0.5)).rgb);
    vec3 rgbB = rgbA * 0.5 + 0.25 * (
        texture(u_Texture, v_TexCoord + dir * -0.5).rgb +
        texture(u_Texture, v_TexCoord + dir * 0.5).rgb);
    float lumaB = dot(rgbB, luma);
    if ((lumaB < lumaMin) || (lumaB > lumaMax))
        FragColor = vec4(rgbA, 1.0);
    else
        FragColor = vec4(rgbB, 1.0);
}
