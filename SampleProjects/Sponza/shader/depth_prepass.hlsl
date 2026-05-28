cbuffer PerFrame : register(b0)
{
    float4x4 ViewProjection;
};

cbuffer PerObject : register(b2)
{
    float4x4 World;
};

struct VSInput
{
    float4 Pos : POSITION;
};

struct PSInput
{
    float4 Pos : SV_POSITION;
};

PSInput vMain(VSInput input)
{
    PSInput output;
    float4 worldPos = mul(input.Pos, World);
    output.Pos = mul(worldPos, ViewProjection);
    return output;
}

void pMain(PSInput input)
{
}
