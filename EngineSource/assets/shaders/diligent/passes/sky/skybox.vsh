// Diligent HLSL: passes/sky/skybox (same as opengl passes/sky/skybox.vert)
// Fullscreen quad: ATTRIB0=position (float2), ATTRIB1=texcoord (float2)
struct VSInput
{
    float2 Position : ATTRIB0;
    float2 TexCoord : ATTRIB1;
};

struct PSInput
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

void main(in VSInput VSIn, out PSInput PSIn)
{
    PSIn.Position = float4(VSIn.Position, 1.0, 1.0);
    PSIn.TexCoord = VSIn.TexCoord;
}
