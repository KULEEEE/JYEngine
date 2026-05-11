cbuffer PerFrame : register(b0)
{
    matrix ViewProjection;
}

cbuffer PerMaterial : register(b1)
{
    float4 BaseColor;
}

Texture2D BaseTexture : register(t0);
SamplerState LinearSampler : register(s0);

struct VS_INPUT {
    float4 Pos : POSITION;
};

struct PS_INPUT {
    float4 Pos : SV_POSITION;
};

PS_INPUT vMain(VS_INPUT input)
{
    PS_INPUT output;
    output.Pos = mul(input.Pos, ViewProjection);
    return output;
}

float4 pMain(PS_INPUT input) : SV_TARGET
{
    float4 texColor = BaseTexture.Sample(LinearSampler, float2(0.5f, 0.5f));
    return texColor * BaseColor;
}
