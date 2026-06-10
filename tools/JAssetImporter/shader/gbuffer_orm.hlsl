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
// ORM 패킹 텍스처 (Bistro/glTF 컨벤션): R=Occlusion, G=Roughness, B=Metalness
Texture2D ORMTexture : register(t2);
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
    float3 WorldPos : TEXCOORD1;
};

PSInput vMain(VSInput input)
{
    PSInput output;
    float4 worldPos = mul(input.Pos, World);
    output.Pos = mul(worldPos, ViewProjection);
    output.WorldNormal = normalize(mul(float4(input.Normal, 0.0f), World).xyz);
    output.UV = input.UV;
    output.WorldPos = worldPos.xyz;
    return output;
}

// 버텍스 탄젠트 없이 화면 공간 미분으로 TBN을 즉석 구성 (Schüler의 cotangent frame).
// ddx/ddy는 메시를 래스터라이즈하는 동안만 유효하므로 G버퍼 패스에서 노멀을 완성해 기록한다.
float3x3 CotangentFrame(float3 normal, float3 worldPos, float2 uv)
{
    float3 dp1 = ddx(worldPos);
    float3 dp2 = ddy(worldPos);
    float2 duv1 = ddx(uv);
    float2 duv2 = ddy(uv);

    float3 dp2perp = cross(dp2, normal);
    float3 dp1perp = cross(normal, dp1);
    float3 tangent = dp2perp * duv1.x + dp1perp * duv2.x;
    float3 bitangent = dp2perp * duv1.y + dp1perp * duv2.y;

    // UV 변화가 없는 픽셀(rsqrt(0) = inf)에서 NaN이 G버퍼로 새지 않게 막는다.
    float invMax = rsqrt(max(max(dot(tangent, tangent), dot(bitangent, bitangent)), 1e-10f));
    return float3x3(tangent * invMax, bitangent * invMax, normal);
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
    float3 orm = ORMTexture.Sample(BaseSampler, input.UV).rgb;
    float occlusion = orm.r;
    float roughness = saturate(orm.g * Roughness);
    float metallic = saturate(orm.b * Metallic);

    // BC5 normal maps store only RG, so Z is reconstructed instead of sampled.
    float2 normalXY = NormalTexture.Sample(BaseSampler, input.UV).rg * 2.0f - 1.0f;
    float normalZ = sqrt(saturate(1.0f - dot(normalXY, normalXY)));
    float3 textureNormal = float3(normalXY, normalZ);
    float3x3 tbn = CotangentFrame(normalize(input.WorldNormal), input.WorldPos, input.UV);
    float3 worldNormal = normalize(mul(textureNormal, tbn));

    output.Albedo = albedo;
    output.Normal = float4(worldNormal * 0.5f + 0.5f, 1.0f);
    output.Material = float4(max(roughness, 0.04f), metallic, occlusion, 1.0f);
    return output;
}
