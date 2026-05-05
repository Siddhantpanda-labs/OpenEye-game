// Diligent HLSL: passes/shadow/shadow (same as opengl passes/shadow/shadow.vert)
struct VSInput
{
    float3 Position : ATTRIB0;
};

cbuffer Constants : register(b0)
{
    row_major float4x4 u_Model;
    row_major float4x4 u_LightSpaceMatrix;
};

void main(in VSInput VSIn, out float4 SV_Position : SV_POSITION)
{
    float4 worldPos = mul(u_Model, float4(VSIn.Position, 1.0));
    SV_Position = mul(u_LightSpaceMatrix, worldPos);
}
