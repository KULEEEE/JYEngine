cbuffer PerFrame : register(b0)
{
    matrix ViewProjection;
}

cbuffer PerMaterial : register(b1)
{
    float4 BaseColor;
}

cbuffer PerObject : register(b2)
{
    matrix World;
}

cbuffer PerLights : register(b3)
{
    float4 LightColorIntensity;
    float4 LightPositionCount;
}

Texture2D BaseTexture : register(t0);
SamplerState LinearSampler : register(s0);

struct VS_INPUT {
    float4 Pos : POSITION;
};

struct PS_INPUT {
    float4 Pos : SV_POSITION;
    float3 WorldPos : TEXCOORD0;
};

PS_INPUT vMain(VS_INPUT input)
{
    PS_INPUT output;
    float4 worldPos = mul(input.Pos, World);
    output.Pos = mul(worldPos, ViewProjection);
    output.WorldPos = worldPos.xyz;
    return output;
}

float4 pMain(PS_INPUT input) : SV_TARGET
{
    float4 texColor = BaseTexture.Sample(LinearSampler, float2(0.5f, 0.5f));
    float lightCount = LightPositionCount.w;
    float3 lightVector = LightPositionCount.xyz - input.WorldPos;
    float distanceFalloff = 1.0f / max(dot(lightVector, lightVector) * 0.08f, 1.0f);
    float lightAmount = lightCount > 0.0f ? LightColorIntensity.a * distanceFalloff : LightColorIntensity.a;
    float3 lighting = 0.25f + LightColorIntensity.rgb * lightAmount;
    return float4((texColor.rgb * BaseColor.rgb) * lighting, texColor.a * BaseColor.a);
}
