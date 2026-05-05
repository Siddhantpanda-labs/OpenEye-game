// Cube vertex shader (Diligent HLSL) - test/cube, with lighting
// ATTRIB0 = position (float3), ATTRIB1 = normal (float3).
// Constants: u_MVP (0), u_Model (64), u_LightDir (128).

cbuffer Constants : register(b0)
{
    float4x4 u_MVP;
    float4x4 u_Model;
    float3 u_LightDir;
    float _pad0;
};

struct VSInput
{
    float3 Position : ATTRIB0;
    float3 Normal   : ATTRIB1;
};

struct PSInput
{
    float4 Position : SV_POSITION;
    float3 WorldNormal : TEXCOORD0;
    float3 WorldLightDir : TEXCOORD1;
};

void main(in VSInput VSIn, out PSInput PSIn)
{
    PSIn.Position = mul(float4(VSIn.Position, 1.0), u_MVP);
    float3 worldN = mul(float4(VSIn.Normal, 0.0), u_Model).xyz;
    PSIn.WorldNormal = normalize(worldN);
    PSIn.WorldLightDir = normalize(u_LightDir);
}
