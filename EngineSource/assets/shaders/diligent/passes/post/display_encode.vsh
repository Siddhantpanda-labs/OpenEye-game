// Fullscreen quad — same as copy.vsh (passes/post/display_encode)

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
    PSIn.Position = float4(VSIn.Position, 0.0, 1.0);
    PSIn.TexCoord = VSIn.TexCoord;
}
