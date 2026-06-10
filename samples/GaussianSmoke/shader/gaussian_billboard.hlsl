cbuffer PerFrame : register(b0)
{
    matrix ViewProjection;
}

cbuffer PerMaterial : register(b1)
{
    float4 SmokeColor;
    float Sharpness;
    float3 Padding;
}

cbuffer PerObject : register(b2)
{
    matrix World;
}

struct VS_INPUT
{
    float4 Pos : POSITION;
    float3 Normal : NORMAL;
    float2 TexCoord : TEXCOORD0;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

PS_INPUT vMain(VS_INPUT input)
{
    PS_INPUT output;

    float4 worldPos = mul(input.Pos, World);
    worldPos.xyz += input.Normal * 0.000001f;

    output.Pos = mul(worldPos, ViewProjection);
    output.TexCoord = input.TexCoord;

    return output;
}

float4 pMain(PS_INPUT input) : SV_TARGET
{
    float2 centeredUv = input.TexCoord * 2.0f - 1.0f;
    float radiusSq = dot(centeredUv, centeredUv);
    float gaussian = exp(-radiusSq * Sharpness);

    // A mathematically pure Gaussian has an infinite tail.  On a billboard
    // that tail becomes a visible square, so the sample trims only the very
    // faint edge and remaps the remaining alpha smoothly.
    float trimmed = saturate((gaussian - 0.035f) / (1.0f - 0.035f));
    float alpha = smoothstep(0.0f, 1.0f, trimmed) * SmokeColor.a;

    return float4(SmokeColor.rgb, alpha);
}
