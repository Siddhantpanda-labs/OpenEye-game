// Diligent HLSL: skinned vertex (passes/forward/skinned)
// ATTRIB0=pos, ATTRIB1=normal, ATTRIB2=uv, ATTRIB7=uv1, ATTRIB5=boneIndices, ATTRIB6=boneWeights

struct VSInput
{
    float3 Position    : ATTRIB0;
    float3 Normal      : ATTRIB1;
    float2 TexCoord    : ATTRIB2;
    float2 TexCoord1   : ATTRIB7;
    int4   BoneIndices : ATTRIB5;
    float4 BoneWeights : ATTRIB6;
};

struct PSInput
{
    float4 Position : SV_POSITION;
    float3 PositionW : TEXCOORD0;
    float3 Normal   : TEXCOORD1;
    float2 TexCoord : TEXCOORD2;
};

#define MAX_BONES 128

cbuffer Constants : register(b0)
{
    // IMPORTANT: Engine uploads GLM matrices (column-major). Do NOT mark these row_major.
    float4x4 u_Model;
    float4x4 u_View;
    float4x4 u_Projection;
    float4x4 u_NormalMatrix;
    float4 u_UVTransform;
    float u_UVRotation;
    float u_TexCoordIndex;
    float u_FlipV;
    float u_pad;
};

cbuffer Bones : register(b1)
{
    // IMPORTANT: Bone matrices are also uploaded as column-major.
    float4x4 u_Bones[MAX_BONES];
};

void main(in VSInput VSIn, out PSInput PSIn)
{
    float4 pos = float4(VSIn.Position, 1.0);
    float3 nrm = VSIn.Normal;
    float4x4 skin = (float4x4)0;
    for (int i = 0; i < 4; i++)
    {
        int idx = VSIn.BoneIndices[i];
        float w = VSIn.BoneWeights[i];
        if (w > 0.0 && idx >= 0 && idx < MAX_BONES)
            skin += u_Bones[idx] * w;
    }
    pos = mul(skin, pos);
    nrm = mul((float3x3)skin, nrm);

    float4 worldPos = mul(u_Model, pos);
    PSIn.PositionW = worldPos.xyz;
    PSIn.Normal = normalize(mul((float3x3)u_NormalMatrix, nrm));

    float2 baseUv = lerp(VSIn.TexCoord, VSIn.TexCoord1, step(0.5, u_TexCoordIndex));
    float2 uv = baseUv * u_UVTransform.zw;
    float c = cos(u_UVRotation);
    float s = sin(u_UVRotation);
    uv = float2(uv.x * c - uv.y * s, uv.x * s + uv.y * c);
    uv += u_UVTransform.xy;
    if (u_FlipV > 0.5) uv.y = 1.0 - uv.y;
    PSIn.TexCoord = uv;

    PSIn.Position = mul(u_Projection, mul(u_View, worldPos));
}
