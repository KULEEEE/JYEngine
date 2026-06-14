// 화면 공간 AO를 단일 채널 타겟에 렌더한다. 결과는 LightingPass가 블러해서 읽는다.
// (인라인 SSAO는 블러가 없어 알갱이가 생기므로 별도 패스로 분리)

cbuffer PerView : register(b0)
{
    float4x4 InverseViewProjection;
    float4 CameraPosition;
};

Texture2D GBufferNormal : register(t0);
Texture2D DepthBuffer : register(t1);

struct VSOut
{
    float4 Pos : SV_POSITION;
};

VSOut vMain(uint vertexID : SV_VertexID)
{
    VSOut output;
    float2 positions[3] =
    {
        float2(-1.0f, -1.0f),
        float2(-1.0f,  3.0f),
        float2( 3.0f, -1.0f)
    };
    output.Pos = float4(positions[vertexID], 0.0f, 1.0f);
    return output;
}

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

float InterleavedGradientNoise(float2 p)
{
    return frac(52.9829189f * frac(0.06711056f * p.x + 0.00583715f * p.y));
}

float pMain(VSOut input) : SV_TARGET
{
    int2 pixel = int2(input.Pos.xy);

    float depth = DepthBuffer.Load(int3(pixel, 0)).r;
    if (depth <= 0.0f)
    {
        return 1.0f; // 하늘 = 차폐 없음
    }

    float3 normal = normalize(GBufferNormal.Load(int3(pixel, 0)).xyz * 2.0f - 1.0f);
    float3 worldPos = ReconstructWorldPosition(pixel, depth);

    uint width;
    uint height;
    DepthBuffer.GetDimensions(width, height);

    const int SAMPLE_COUNT = 16;
    const float worldRadius = 0.5f;   // 월드 공간 AO 반경(m). 거리와 무관하게 일정한 발자국.
    const float worldRange = 1.0f;
    const float bias = 0.04f;
    const float strength = 1.0f;

    // 월드 반경 → 화면 픽셀 반경 환산(fovY=60° 가정). 멀수록 픽셀 반경이 작아져 원거리 얼룩을 막는다.
    const float fovY = 1.04719755f; // 60도
    float focalY = (float(height) * 0.5f) / tan(fovY * 0.5f);
    float viewDist = max(length(worldPos - CameraPosition.xyz), 0.1f);
    float radiusPixels = clamp(worldRadius * focalY / viewDist, 3.0f, 48.0f);

    float rnd = InterleavedGradientNoise(float2(pixel)) * 6.2831853f;
    float occlusion = 0.0f;
    float valid = 0.0f;

    [loop]
    for (int i = 0; i < SAMPLE_COUNT; ++i)
    {
        float t = (float(i) + 0.5f) / float(SAMPLE_COUNT);
        float angle = rnd + float(i) * 2.39996323f; // 황금각
        float2 dir = float2(cos(angle), sin(angle)) * radiusPixels * sqrt(t);
        int2 sp = pixel + int2(dir);
        if (sp.x < 0 || sp.y < 0 || sp.x >= (int)width || sp.y >= (int)height)
        {
            continue;
        }

        float sd = DepthBuffer.Load(int3(sp, 0)).r;
        if (sd <= 0.0f)
        {
            continue;
        }

        float3 samplePos = ReconstructWorldPosition(sp, sd);
        float3 v = samplePos - worldPos;
        float dist = length(v);
        if (dist < 1e-3f)
        {
            continue;
        }

        float ndotv = dot(normal, v / dist);
        float rangeCheck = saturate(1.0f - (dist - worldRange) / worldRange);
        occlusion += step(bias, ndotv) * rangeCheck;
        valid += 1.0f;
    }

    occlusion = valid > 0.0f ? occlusion / valid : 0.0f;
    return saturate(1.0f - occlusion * strength);
}
