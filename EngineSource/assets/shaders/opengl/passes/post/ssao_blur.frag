#version 430 core

in vec2 v_TexCoord;
out float FragColor;

uniform sampler2D u_SSAO;
uniform sampler2D u_Depth;
uniform vec2 u_Axis;
uniform vec2 u_Resolution;
uniform float u_NearPlane = 0.1;
uniform float u_FarPlane = 1000.0;
uniform float u_FarPlaneOverEdgeDistance = 1600.0;
uniform int u_SampleCount = 6;
uniform float u_Kernel[16];

float linearizeDepth(float depthSample) {
    float zNdc = depthSample * 2.0 - 1.0;
    return (2.0 * u_NearPlane * u_FarPlane) / (u_FarPlane + u_NearPlane - zNdc * (u_FarPlane - u_NearPlane));
}

float bilateralWeight(float centerDepth, float sampleDepth) {
    float diff = (sampleDepth - centerDepth) * u_FarPlaneOverEdgeDistance;
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
    float sum = centerAo * u_Kernel[0];
    float totalWeight = u_Kernel[0];

    vec2 texelSize = u_Axis / u_Resolution;

    for (int i = 1; i < u_SampleCount && i < 16; ++i) {
        float w = u_Kernel[i];
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
