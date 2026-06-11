static const int MAX_RENDER_LIGHTS = 8;

cbuffer PerLights : register(b0)
{
    float4 LightColorIntensities[MAX_RENDER_LIGHTS];
    float4 LightPositions[MAX_RENDER_LIGHTS]; // xyz: world position, w: range
    float4 LightInfo;
};

cbuffer PerView : register(b1)
{
    float4x4 InverseViewProjection;
    float4 CameraPosition;
};

static const int CASCADE_COUNT = 4;

cbuffer PerShadow : register(b2)
{
    float4x4 ShadowViewProjections[CASCADE_COUNT];
    float4 CascadeSplits; // 카메라 거리 기준 cascade 경계
    float4 CascadeBiases; // reverse-Z depth 단위 bias (cascade별)
    float4 ShadowParams;  // x: enabled, y: max shadow distance
};

Texture2D GBufferAlbedo : register(t0);
Texture2D GBufferNormal : register(t1);
Texture2D GBufferMaterial : register(t2);
Texture2D DepthBuffer : register(t3);
Texture2DArray ShadowMap : register(t4);
// 이름에 Shadow가 들어가면 엔진이 GREATER_EQUAL 비교 샘플러로 생성함 (reverse-Z 규약)
SamplerComparisonState ShadowSampler : register(s0);

static const float PI = 3.14159265f;
static const float SHADOW_PCF_RADIUS = 1.25f;

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

// reverse-Z depth로부터 world position 복원
float3 ReconstructWorldPosition(int2 pixel, float depth)
{
    uint width;
    uint height;
    DepthBuffer.GetDimensions(width, height);

    float2 uv = (float2(pixel) + 0.5f) / float2(width, height);
    float2 ndcXY = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    float4 worldPos = mul(float4(ndcXY, depth, 1.0f), InverseViewProjection);
    return worldPos.xyz / max(worldPos.w, 0.0001f);
}

// inverse-square 감쇠 + range 윈도잉 (UE/Frostbite 스타일)
float DistanceAttenuation(float dist, float range)
{
    float ratio = dist / max(range, 0.001f);
    float window = pow(saturate(1.0f - ratio * ratio * ratio * ratio), 2.0f);
    return window / (dist * dist + 1.0f);
}

// ACES filmic 근사 (Narkowicz 2015). Reinhard보다 대비/채도 보존이 좋음.
float3 ACESFilm(float3 color)
{
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return saturate((color * (a * color + b)) / (color * (c * color + d) + e));
}

// directional light의 cascade shadow. 1 = lit, 0 = 그림자.
float SampleShadowPCF(float2 shadowUV, int cascade, float referenceDepth)
{
    uint width;
    uint height;
    uint layers;
    uint levels;
    ShadowMap.GetDimensions(0, width, height, layers, levels);

    float2 texelSize = 1.0f / float2(width, height);
    float2 offset = texelSize * SHADOW_PCF_RADIUS;
    float layer = (float)cascade;

    float shadow = 0.0f;
    shadow += ShadowMap.SampleCmpLevelZero(ShadowSampler, float3(shadowUV + float2(-offset.x, -offset.y), layer), referenceDepth);
    shadow += ShadowMap.SampleCmpLevelZero(ShadowSampler, float3(shadowUV + float2( offset.x, -offset.y), layer), referenceDepth);
    shadow += ShadowMap.SampleCmpLevelZero(ShadowSampler, float3(shadowUV + float2(-offset.x,  offset.y), layer), referenceDepth);
    shadow += ShadowMap.SampleCmpLevelZero(ShadowSampler, float3(shadowUV + float2( offset.x,  offset.y), layer), referenceDepth);
    return shadow * 0.25f;
}

float SampleCascadeShadow(float3 worldPos, float viewDistance)
{
    if (ShadowParams.x < 0.5f || viewDistance > ShadowParams.y)
    {
        return 1.0f;
    }

    int cascade = 3;
    if (viewDistance < CascadeSplits.x)
    {
        cascade = 0;
    }
    else if (viewDistance < CascadeSplits.y)
    {
        cascade = 1;
    }
    else if (viewDistance < CascadeSplits.z)
    {
        cascade = 2;
    }

    float4 shadowPos = mul(float4(worldPos, 1.0f), ShadowViewProjections[cascade]);
    shadowPos.xyz /= max(shadowPos.w, 0.0001f);
    float2 shadowUV = float2(shadowPos.x * 0.5f + 0.5f, 0.5f - shadowPos.y * 0.5f);

    // reverse-Z: bias를 더해 수신면을 라이트 쪽으로 살짝 당겨 acne를 줄임.
    // 맵 밖 uv는 border 샘플러가 ref >= 0으로 항상 lit 처리.
    float referenceDepth = saturate(shadowPos.z + CascadeBiases[cascade]);
    return SampleShadowPCF(shadowUV, cascade, referenceDepth);
}

float4 pMain(VS_OUTPUT input) : SV_TARGET
{
    int2 pixel = int2(input.Pos.xy);
    float4 albedoSample = GBufferAlbedo.Load(int3(pixel, 0));

    // reverse-Z: depth 0 == far plane. 아무것도 그려지지 않은 픽셀은 라이팅을 건너뛴다.
    float depth = DepthBuffer.Load(int3(pixel, 0)).r;
    if (depth <= 0.0f)
    {
        return float4(albedoSample.rgb, albedoSample.a);
    }

    float3 albedo = pow(saturate(albedoSample.rgb), 2.2f);
    float3 normal = normalize(GBufferNormal.Load(int3(pixel, 0)).xyz * 2.0f - 1.0f);
    float4 material = GBufferMaterial.Load(int3(pixel, 0));

    float roughness = saturate(material.r);
    float metallic = saturate(material.g);
    float occlusion = saturate(material.b);

    float3 worldPos = ReconstructWorldPosition(pixel, depth);
    float3 viewDir = normalize(CameraPosition.xyz - worldPos);
    float3 f0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);
    // 베이크된 AO는 간접광(ambient)에만 적용함. 직접광은 섀도우맵이 가림을 담당.
    float3 ambient = albedo * 0.28f * (1.0f - metallic) * occlusion;
    float3 direct = float3(0.0f, 0.0f, 0.0f);

    // 모든 directional light가 첫 directional 기준의 cascade shadow를 공유함
    float directionalShadow = SampleCascadeShadow(worldPos, distance(CameraPosition.xyz, worldPos));

    int lightCount = min((int)LightInfo.x, MAX_RENDER_LIGHTS);
    [loop]
    for (int i = 0; i < lightCount; ++i)
    {
        // w <= 0이면 directional (xyz = 빛이 진행하는 방향, 감쇠 없음), w > 0이면 point (xyz = 위치, w = range)
        float4 positionRange = LightPositions[i];
        float3 lightDir;
        float attenuation;
        if (positionRange.w <= 0.0f)
        {
            lightDir = normalize(-positionRange.xyz);
            attenuation = directionalShadow;
        }
        else
        {
            float3 lightVec = positionRange.xyz - worldPos;
            float dist = length(lightVec);
            lightDir = lightVec / max(dist, 0.0001f);
            attenuation = DistanceAttenuation(dist, positionRange.w);
        }

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
        direct += (kd * albedo / PI + specular) * lightColor * ndotl * attenuation;
    }

    float3 color = ambient + direct;

    color = ACESFilm(color);
    color = pow(saturate(color), 1.0f / 2.2f);
    return float4(color, albedoSample.a);
}
