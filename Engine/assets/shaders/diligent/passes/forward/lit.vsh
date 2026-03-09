// Diligent HLSL: lit vertex (passes/forward/lit) - same as basic
struct VSInput
{
    float3 Position : ATTRIB0;
    float3 Normal   : ATTRIB1;
    float2 TexCoord : ATTRIB2;
};

struct PSInput
{
    float4 Position : SV_POSITION;
    float3 PositionW : TEXCOORD0;
    float3 Normal   : TEXCOORD1;
    float2 TexCoord : TEXCOORD2;
};

cbuffer Constants : register(b0)
{
    // IMPORTANT: Engine uploads GLM matrices (column-major). Do NOT mark these row_major.
    float4x4 u_Model;
    float4x4 u_View;
    float4x4 u_Projection;
    float4x4 u_NormalMatrix;
};

void main(in VSInput VSIn, out PSInput PSIn)
{
    float4 worldPos = mul(u_Model, float4(VSIn.Position, 1.0));
    PSIn.PositionW = worldPos.xyz;
    PSIn.Normal = normalize(mul((float3x3)u_NormalMatrix, VSIn.Normal));
    PSIn.TexCoord = float2(VSIn.TexCoord.x, 1.0 - VSIn.TexCoord.y);
    PSIn.Position = mul(u_Projection, mul(u_View, worldPos));
}
