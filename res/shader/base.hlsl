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

static const int MAX_RENDER_LIGHTS = 8;

cbuffer PerLights : register(b3)
{
    float4 LightColorIntensities[MAX_RENDER_LIGHTS];
    float4 LightPositions[MAX_RENDER_LIGHTS];
    float4 LightInfo;
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
    float3 baseColor = texColor.rgb * BaseColor.rgb;
    float3 litColor = baseColor * 0.15f;

    int lightCount = min((int)LightInfo.x, MAX_RENDER_LIGHTS);
    [loop]
    for (int i = 0; i < lightCount; ++i)
    {
        float3 lightVector = LightPositions[i].xyz - input.WorldPos;
        float lightDistance = length(lightVector);
        float distanceFalloff = saturate(1.0f - lightDistance / 10.0f);
        float lightAmount = saturate(distanceFalloff * LightColorIntensities[i].a);

        // Temporary debug tint until normal-based lighting exists.
        litColor += baseColor * lightAmount;
        litColor += LightColorIntensities[i].rgb * (lightAmount * 0.35f);
    }
    return float4(litColor, texColor.a * BaseColor.a);
}
