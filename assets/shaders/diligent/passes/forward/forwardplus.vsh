// Diligent HLSL: passes/forward/forwardplus (same as opengl passes/forward/forwardplus.vert)
// Non-skinned: ATTRIB0=pos, 1=normal, 2=uv, 3=tangent
struct VSInput
{
    float3 Position  : ATTRIB0;
    float3 Normal   : ATTRIB1;
    float2 TexCoord : ATTRIB2;
    float3 Tangent  : ATTRIB3;
};

struct PSInput
{
    float4 Position   : SV_POSITION;
    float3 WorldPos   : TEXCOORD0;
    float3 ViewPos    : TEXCOORD1;
    float3 WorldNormal : TEXCOORD2;
    float2 TexCoord   : TEXCOORD3;
    float3x3 TBN      : TEXCOORD4;
};

cbuffer Constants : register(b0)
{
    row_major float4x4 u_Model;
    row_major float4x4 u_View;
    row_major float4x4 u_Projection;
    row_major float4x4 u_NormalMatrix;
    float4 u_UVTransform;
    float u_UVRotation;
    float u_TexCoordIndex;
    float u_FlipV;
    float _pad;
};

void main(in VSInput VSIn, out PSInput PSIn)
{
    float4 worldPos = mul(u_Model, float4(VSIn.Position, 1.0));
    PSIn.WorldPos = worldPos.xyz;
    float4 viewPos = mul(u_View, worldPos);
    PSIn.ViewPos = viewPos.xyz;

    float3x3 normalMatrix = (float3x3)u_NormalMatrix;
    float3 worldNormal = normalize(mul(normalMatrix, VSIn.Normal));
    float3 worldTangent;
    if (length(VSIn.Tangent) < 0.001)
    {
        float3 up = abs(worldNormal.y) < 0.999 ? float3(0, 1, 0) : float3(1, 0, 0);
        worldTangent = normalize(cross(up, worldNormal));
    }
    else
        worldTangent = normalize(mul(normalMatrix, VSIn.Tangent));
    float3 worldBitangent = normalize(cross(worldNormal, worldTangent));
    PSIn.WorldNormal = worldNormal;
    PSIn.TBN = float3x3(worldTangent, worldBitangent, worldNormal);

    float2 uv = VSIn.TexCoord * u_UVTransform.zw;
    float c = cos(u_UVRotation);
    float s = sin(u_UVRotation);
    uv = float2(uv.x * c - uv.y * s, uv.x * s + uv.y * c);
    uv += u_UVTransform.xy;
    if (u_FlipV > 0.5) uv.y = 1.0 - uv.y;
    PSIn.TexCoord = uv;

    PSIn.Position = mul(u_Projection, viewPos);
}
