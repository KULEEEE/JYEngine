static const int MAX_RENDER_LIGHTS = 8;

cbuffer PerLights : register(b0)
{
    float4 LightColorIntensities[MAX_RENDER_LIGHTS];
    float4 LightPositions[MAX_RENDER_LIGHTS];
    float4 LightInfo;
};

Texture2D GBufferAlbedo : register(t0);
Texture2D GBufferNormal : register(t1);
Texture2D GBufferMaterial : register(t2);

static const float PI = 3.14159265f;

struct VS_OUTPUT
{
    float4 Pos : SV_POSITION;
};

VS_OUTPUT vMain(uint vertexID : SV_VertexID)
{
    VS_OUTPUT output;
    float2 positions[3] =
    {
        float2(-1.0f, -1.0f),
        float2(-1.0f,  3.0f),
        float2( 3.0f, -1.0f)
    };

    output.Pos = float4(positions[vertexID], 0.0f, 1.0f);
    return output;
}

float DistributionGGX(float3 normal, float3 halfVector, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float ndoth = max(dot(normal, halfVector), 0.0f);
    float ndoth2 = ndoth * ndoth;
    float denom = ndoth2 * (a2 - 1.0f) + 1.0f;
    return a2 / max(PI * denom * denom, 0.0001f);
}

float GeometrySchlickGGX(float ndotv, float roughness)
{
    float r = roughness + 1.0f;
    float k = (r * r) / 8.0f;
    return ndotv / max(ndotv * (1.0f - k) + k, 0.0001f);
}

float GeometrySmith(float3 normal, float3 viewDir, float3 lightDir, float roughness)
{
    float ndotv = max(dot(normal, viewDir), 0.0f);
    float ndotl = max(dot(normal, lightDir), 0.0f);
    return GeometrySchlickGGX(ndotv, roughness) * GeometrySchlickGGX(ndotl, roughness);
}

float3 FresnelSchlick(float cosTheta, float3 f0)
{
    return f0 + (1.0f - f0) * pow(saturate(1.0f - cosTheta), 5.0f);
}

float4 pMain(VS_OUTPUT input) : SV_TARGET
{
    int2 pixel = int2(input.Pos.xy);
    float4 albedoSample = GBufferAlbedo.Load(int3(pixel, 0));
    float3 albedo = pow(saturate(albedoSample.rgb), 2.2f);
    float3 normal = normalize(GBufferNormal.Load(int3(pixel, 0)).xyz * 2.0f - 1.0f);
    float4 material = GBufferMaterial.Load(int3(pixel, 0));

    float roughness = saturate(material.r);
    float metallic = saturate(material.g);

    float3 viewDir = normalize(float3(0.0f, 0.25f, -1.0f));
    float3 f0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);
    float3 ambient = albedo * 0.28f * (1.0f - metallic);
    float3 direct = float3(0.0f, 0.0f, 0.0f);

    int lightCount = min((int)LightInfo.x, MAX_RENDER_LIGHTS);
    [loop]
    for (int i = 0; i < lightCount; ++i)
    {
        float3 lightDir = normalize(LightPositions[i].xyz);
        float3 halfVector = normalize(viewDir + lightDir);
        float3 lightColor = LightColorIntensities[i].rgb * LightColorIntensities[i].a;
        float ndotl = max(dot(normal, lightDir), 0.0f);

        float ndf = DistributionGGX(normal, halfVector, roughness);
        float geometry = GeometrySmith(normal, viewDir, lightDir, roughness);
        float3 fresnel = FresnelSchlick(max(dot(halfVector, viewDir), 0.0f), f0);

        float3 numerator = ndf * geometry * fresnel;
        float denominator = max(4.0f * max(dot(normal, viewDir), 0.0f) * ndotl, 0.0001f);
        float3 specular = numerator / denominator;

        float3 ks = fresnel;
        float3 kd = (1.0f - ks) * (1.0f - metallic);
        direct += (kd * albedo / PI + specular) * lightColor * ndotl;
    }

    float3 color = ambient + direct;

    color = color / (color + 1.0f);
    color = pow(saturate(color), 1.0f / 2.2f);
    return float4(color, albedoSample.a);
}
