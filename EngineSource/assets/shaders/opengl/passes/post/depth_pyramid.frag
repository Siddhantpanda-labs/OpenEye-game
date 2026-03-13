#version 430 core

in vec2 v_TexCoord;
layout(location = 0) out vec4 o_Color;

uniform sampler2D u_SourceDepth;  // g_Depth for level 0
uniform sampler2D u_Pyramid;      // pyramid texture for level >= 1 (sample previous mip)
uniform int u_Level;
uniform vec2 u_SourceSize;        // size of source (for level 0 = resolution; for level >= 1 = size of mip Level-1)
uniform float u_NearPlane;
uniform float u_FarPlane;

float linearizeDepth(float depthSample) {
    float zNdc = depthSample * 2.0 - 1.0;
    float linear = (2.0 * u_NearPlane * u_FarPlane) / (u_FarPlane + u_NearPlane - zNdc * (u_FarPlane - u_NearPlane));
    return -linear;
}

void main() {
    if (u_Level == 0) {
        float d = texture(u_SourceDepth, v_TexCoord).r;
        float linear = linearizeDepth(d);
        o_Color = vec4(linear, 0.0, 0.0, 1.0);
    } else {
        float mip = float(u_Level - 1);
        float invW = 1.0 / u_SourceSize.x;
        float invH = 1.0 / u_SourceSize.y;
        vec2 tc = (floor(v_TexCoord * 2.0) + 0.5) * vec2(invW, invH);
        float d0 = textureLod(u_Pyramid, tc, mip).r;
        float d1 = textureLod(u_Pyramid, tc + vec2(invW, 0.0), mip).r;
        float d2 = textureLod(u_Pyramid, tc + vec2(0.0, invH), mip).r;
        float d3 = textureLod(u_Pyramid, tc + vec2(invW, invH), mip).r;
        float minD = min(min(d0, d1), min(d2, d3));
        o_Color = vec4(minD, 0.0, 0.0, 1.0);
    }
}
