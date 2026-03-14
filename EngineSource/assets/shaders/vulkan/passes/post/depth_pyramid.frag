#version 450

layout(location = 0) in vec2 v_TexCoord;
layout(location = 0) out vec4 o_Color;

layout(binding = 0) uniform sampler2D u_SourceDepth;
layout(binding = 1) uniform sampler2D u_Pyramid;

layout(push_constant, std430) uniform Push {
    int u_Level;
    vec2 u_SourceSize;
    float u_NearPlane;
    float u_FarPlane;
} pc;

float linearizeDepth(float depthSample) {
    float zNdc = depthSample * 2.0 - 1.0;
    float linear = (2.0 * pc.u_NearPlane * pc.u_FarPlane) / (pc.u_FarPlane + pc.u_NearPlane - zNdc * (pc.u_FarPlane - pc.u_NearPlane));
    return -linear;
}

void main() {
    if (pc.u_Level == 0) {
        float d = texture(u_SourceDepth, v_TexCoord).r;
        float linear = linearizeDepth(d);
        o_Color = vec4(linear, 0.0, 0.0, 1.0);
    } else {
        float mip = float(pc.u_Level - 1);
        float invW = 1.0 / pc.u_SourceSize.x;
        float invH = 1.0 / pc.u_SourceSize.y;
        vec2 tc = (floor(v_TexCoord * 2.0) + 0.5) * vec2(invW, invH);
        float d0 = textureLod(u_Pyramid, tc, mip).r;
        float d1 = textureLod(u_Pyramid, tc + vec2(invW, 0.0), mip).r;
        float d2 = textureLod(u_Pyramid, tc + vec2(0.0, invH), mip).r;
        float d3 = textureLod(u_Pyramid, tc + vec2(invW, invH), mip).r;
        float minD = min(min(d0, d1), min(d2, d3));
        o_Color = vec4(minD, 0.0, 0.0, 1.0);
    }
}
