// Diligent HLSL: passes/shadow/shadow_skinned (same as opengl passes/shadow/shadow_skinned.vert)
struct VSInput
{
    float3 Position    : ATTRIB0;
    int4   BoneIndices : ATTRIB5;
    float4 BoneWeights : ATTRIB6;
};

#define MAX_BONES 128

cbuffer Constants : register(b0)
{
    row_major float4x4 u_Model;
    row_major float4x4 u_LightSpaceMatrix;
};

cbuffer Bones : register(b3)
{
    row_major float4x4 u_Bones[MAX_BONES];
};

void main(in VSInput VSIn, out float4 SV_Position : SV_POSITION)
{
    float4 pos = float4(VSIn.Position, 1.0);
    float4x4 skin = (float4x4)0;
    for (int i = 0; i < 4; i++)
    {
        int idx = VSIn.BoneIndices[i];
        float w = VSIn.BoneWeights[i];
        if (w > 0.0 && idx >= 0 && idx < MAX_BONES)
            skin += u_Bones[idx] * w;
    }
    pos = mul(skin, pos);
    float4 worldPos = mul(u_Model, pos);
    SV_Position = mul(u_LightSpaceMatrix, worldPos);
}
