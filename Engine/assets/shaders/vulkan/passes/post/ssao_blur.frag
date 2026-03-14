#version 450

layout(location = 0) in vec2 v_TexCoord;
layout(location = 0) out float FragColor;

layout(binding = 0) uniform sampler2D u_SSAO;
layout(binding = 1) uniform sampler2D u_Depth;

layout(push_constant, std430) uniform Push {
    vec2 u_Axis;
    vec2 u_Resolution;
    float u_NearPlane;
    float u_FarPlane;
    float u_FarPlaneOverEdgeDistance;
    int u_SampleCount;
    float u_Kernel[16];
} pc;

float linearizeDepth(float depthSample) {
    float zNdc = depthSample * 2.0 - 1.0;
    return (2.0 * pc.u_NearPlane * pc.u_FarPlane) / (pc.u_FarPlane + pc.u_NearPlane - zNdc * (pc.u_FarPlane - pc.u_NearPlane));
}

float bilateralWeight(float centerDepth, float sampleDepth) {
    float diff = (sampleDepth - centerDepth) * pc.u_FarPlaneOverEdgeDistance;
    return max(0.0, 1.0 - diff * diff);
}

void main() {
    float centerAo = texture(u_SSAO, v_TexCoord).r;
    float centerDepthSample = texture(u_Depth, v_TexCoord).r;

    if (centerDepthSample >= 0.9999) {
        FragColor = centerAo;
        return;
    }

    float centerLinear = linearizeDepth(centerDepthSample);
    float sum = centerAo * pc.u_Kernel[0];
    float totalWeight = pc.u_Kernel[0];

    vec2 texelSize = pc.u_Axis / pc.u_Resolution;

    for (int i = 1; i < pc.u_SampleCount && i < 16; ++i) {
        float w = pc.u_Kernel[i];
        vec2 offset = float(i) * texelSize;

        vec2 uvP = v_TexCoord + offset;
        float aoP = texture(u_SSAO, uvP).r;
        float depthP = texture(u_Depth, uvP).r;
        float linearP = (depthP >= 0.9999) ? centerLinear : linearizeDepth(depthP);
        float bwP = w * bilateralWeight(centerLinear, linearP);
        sum += aoP * bwP;
        totalWeight += bwP;

        vec2 uvN = v_TexCoord - offset;
        float aoN = texture(u_SSAO, uvN).r;
        float depthN = texture(u_Depth, uvN).r;
        float linearN = (depthN >= 0.9999) ? centerLinear : linearizeDepth(depthN);
        float bwN = w * bilateralWeight(centerLinear, linearN);
        sum += aoN * bwN;
        totalWeight += bwN;
    }

    FragColor = totalWeight > 0.0 ? (sum / totalWeight) : centerAo;
}
