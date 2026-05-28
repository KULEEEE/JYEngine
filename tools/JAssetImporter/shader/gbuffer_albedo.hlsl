cbuffer PerFrame : register(b0)
{
    float4x4 ViewProjection;
};

cbuffer PerMaterial : register(b1)
{
    float4 BaseColor;
    float Roughness;
    float Metallic;
    float2 Padding;
};

cbuffer PerObject : register(b2)
{
    float4x4 World;
};

Texture2D BaseTexture : register(t0);
Texture2D NormalTexture : register(t1);
Texture2D RoughnessTexture : register(t2);
Texture2D MetallicTexture : register(t3);
SamplerState BaseSampler : register(s0);

struct VSInput
{
    float4 Pos : POSITION;
    float3 Normal : NORMAL;
    float2 UV : TEXCOORD0;
};

struct PSInput
{
    float4 Pos : SV_POSITION;
    float3 WorldNormal : NORMAL;
    float2 UV : TEXCOORD0;
};

PSInput vMain(VSInput input)
{
    PSInput output;
    float4 worldPos = mul(input.Pos, World);
    output.Pos = mul(worldPos, ViewProjection);
    output.WorldNormal = normalize(mul(float4(input.Normal, 0.0f), World).xyz);
    output.UV = input.UV;
    return output;
}

struct GBufferOutput
{
    float4 Albedo : SV_TARGET0;
    float4 Normal : SV_TARGET1;
    float4 Material : SV_TARGET2;
};

GBufferOutput pMain(PSInput input)
{
    GBufferOutput output;

    float4 albedo = BaseTexture.Sample(BaseSampler, input.UV) * BaseColor;
    float roughness = saturate(RoughnessTexture.Sample(BaseSampler, input.UV).r * Roughness);
    float metallic = saturate(MetallicTexture.Sample(BaseSampler, input.UV).r * Metallic);

    // Tangents are not in the render resource yet, so this is a light approximation.
    // Proper tangent-space normal mapping should replace this when tangent buffers are added.
    float3 textureNormal = normalize(NormalTexture.Sample(BaseSampler, input.UV).xyz * 2.0f - 1.0f);
    float3 worldNormal = normalize(input.WorldNormal + textureNormal * 0.15f);

    output.Albedo = albedo;
    output.Normal = float4(worldNormal * 0.5f + 0.5f, 1.0f);
    output.Material = float4(max(roughness, 0.04f), metallic, 1.0f, 1.0f);
    return output;
}
