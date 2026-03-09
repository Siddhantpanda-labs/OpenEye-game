// Diligent HLSL: passes/deferred/lighting (fullscreen quad, same as opengl passes/deferred/lighting.vert)
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
    PSIn.TexCoord = VSIn.TexCoord;
    PSIn.Position = float4(VSIn.Position, 0.0, 1.0);
}
