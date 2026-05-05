// Diligent HLSL: passes/deferred/gbuffer_skinned (same logic as opengl passes/deferred/gbuffer_skinned.vert)
// ATTRIB0=pos, 1=normal, 2=uv, 7=uv1, 3=tangent, 5=boneIndices, 6=boneWeights

struct VSInput
{
    float3 Position    : ATTRIB0;
    float3 Normal     : ATTRIB1;
    float2 TexCoord   : ATTRIB2;
    float2 TexCoord1  : ATTRIB7;
    float3 Tangent    : ATTRIB3;
    int4   BoneIndices : ATTRIB5;
    float4 BoneWeights : ATTRIB6;
};

struct PSInput
{
    float4 Position   : SV_POSITION;
    float3 WorldPos   : TEXCOORD0;
    float3 WorldNormal : TEXCOORD1;
    float2 TexCoord   : TEXCOORD2;
    float2 Velocity   : TEXCOORD3;
    float3x3 TBN      : TEXCOORD4;
};

#define MAX_BONES 128

cbuffer Constants : register(b0)
{
    row_major float4x4 u_Model;
    row_major float4x4 u_View;
    row_major float4x4 u_Projection;
    row_major float4x4 u_PrevView;
    row_major float4x4 u_PrevProjection;
    row_major float4x4 u_NormalMatrix;
    float4 u_UVTransform;
    float u_UVRotation;
    float u_TexCoordIndex;
    float u_FlipV;
    float _pad0;
};

cbuffer Bones : register(b3)
{
    row_major float4x4 u_Bones[MAX_BONES];
};

void main(in VSInput VSIn, out PSInput PSIn)
{
    float4 pos = float4(VSIn.Position, 1.0);
    float3 nrm = VSIn.Normal;
    float3 tang = VSIn.Tangent;

    float4x4 skin = (float4x4)0;
    float totalWeight = 0.0;
    for (int i = 0; i < 4; i++)
    {
        int idx = VSIn.BoneIndices[i];
        float w = VSIn.BoneWeights[i];
        if (w > 0.0 && idx >= 0 && idx < MAX_BONES)
        {
            skin += u_Bones[idx] * w;
            totalWeight += w;
        }
    }
    if (totalWeight <= 0.0)
        skin = (float4x4)0;
    else
    {
        pos = mul(skin, pos);
        nrm = mul((float3x3)skin, nrm);
        tang = mul((float3x3)skin, tang);
    }

    float4 worldPos = mul(u_Model, pos);
    float3x3 normalMatrix = (float3x3)u_NormalMatrix;
    float3 worldNormal = normalize(mul(normalMatrix, nrm));
    float3 worldTangent;
    if (length(tang) < 0.001)
    {
        float3 up = abs(worldNormal.y) < 0.999 ? float3(0, 1, 0) : float3(1, 0, 0);
        worldTangent = normalize(cross(up, worldNormal));
    }
    else
        worldTangent = normalize(mul(normalMatrix, tang));

    PSIn.WorldPos = worldPos.xyz;
    PSIn.WorldNormal = worldNormal;

    float2 baseUv = lerp(VSIn.TexCoord, VSIn.TexCoord1, step(0.5, u_TexCoordIndex));
    float2 uv = baseUv * u_UVTransform.zw;
    float c = cos(u_UVRotation);
    float s = sin(u_UVRotation);
    uv = float2(uv.x * c - uv.y * s, uv.x * s + uv.y * c);
    uv += u_UVTransform.xy;
    if (u_FlipV > 0.5) uv.y = 1.0 - uv.y;
    PSIn.TexCoord = uv;

    float4 currentPos = mul(u_Projection, mul(u_View, worldPos));
    float4 prevPos = mul(u_PrevProjection, mul(u_PrevView, worldPos));
    float2 currentNDC = currentPos.xy / currentPos.w;
    float2 prevNDC = prevPos.xy / prevPos.w;
    PSIn.Velocity = currentNDC - prevNDC;

    float3 T = normalize(worldTangent - dot(worldTangent, worldNormal) * worldNormal);
    float3 B = cross(worldNormal, T);
    PSIn.TBN = float3x3(T, B, worldNormal);

    PSIn.Position = currentPos;
}
