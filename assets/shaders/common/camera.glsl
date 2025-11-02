#ifndef MAX_CASCADES
#define MAX_CASCADES 4
#endif

layout(std140, binding = 0) uniform FrameUniforms {
    mat4 u_ViewMatrix;
    mat4 u_ProjectionMatrix;
    mat4 u_InverseViewMatrix;
    mat4 u_InverseProjectionMatrix;
    vec4 u_ViewPos_Time; // xyz = view pos, w = time
    float u_CascadeSplits[MAX_CASCADES + 1];
};

// Helpers
#define u_ViewPos (u_ViewPos_Time.xyz)

