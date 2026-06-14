// 캡처된 환경 큐브를 roughness별로 prefilter한다(split-sum의 specular 항).
// 면×밉마다 full-screen 1패스. mip이 클수록 roughness↑ → 더 흐린 반사.

cbuffer PerFace : register(b0)
{
    float4 FaceForward; // 면 중심 방향
    float4 FaceRight;   // +u
    float4 FaceUp;      // +v
    float4 Params;      // x = roughness
};

TextureCube EnvCube : register(t0);
SamplerState EnvSampler : register(s0);

static const float PI = 3.14159265359;

struct VSOut
{
    float4 Pos : SV_POSITION;
    float2 UV  : TEXCOORD0;
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
    float2 p = positions[vertexID];
    output.Pos = float4(p, 0.0f, 1.0f);
    output.UV = float2((p.x + 1.0f) * 0.5f, 1.0f - (p.y + 1.0f) * 0.5f);
    return output;
}

float RadicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}

float2 Hammersley(uint i, uint n)
{
    return float2(float(i) / float(n), RadicalInverse_VdC(i));
}

float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness)
{
    float a = roughness * roughness;
    float phi = 2.0f * PI * Xi.x;
    float cosTheta = sqrt((1.0f - Xi.y) / (1.0f + (a * a - 1.0f) * Xi.y));
    float sinTheta = sqrt(1.0f - cosTheta * cosTheta);

    float3 H = float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);

    float3 up = abs(N.z) < 0.999f ? float3(0.0f, 0.0f, 1.0f) : float3(1.0f, 0.0f, 0.0f);
    float3 tangentX = normalize(cross(up, N));
    float3 tangentY = cross(N, tangentX);
    return normalize(tangentX * H.x + tangentY * H.y + N * H.z);
}

float4 pMain(VSOut input) : SV_TARGET
{
    float2 ndc = float2(input.UV.x * 2.0f - 1.0f, 1.0f - input.UV.y * 2.0f);
    float3 N = normalize(FaceForward.xyz + ndc.x * FaceRight.xyz + ndc.y * FaceUp.xyz);
    // split-sum 근사: V = R = N 로 둔다.
    float3 R = N;
    float3 V = N;

    float roughness = Params.x;

    float3 prefiltered = float3(0.0f, 0.0f, 0.0f);
    float totalWeight = 0.0f;
    const uint SAMPLE_COUNT = 256u;
    for (uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        float2 Xi = Hammersley(i, SAMPLE_COUNT);
        float3 H = ImportanceSampleGGX(Xi, N, roughness);
        float3 L = normalize(2.0f * dot(V, H) * H - V);

        float ndotl = max(dot(N, L), 0.0f);
        if (ndotl > 0.0f)
        {
            prefiltered += EnvCube.SampleLevel(EnvSampler, L, 0).rgb * ndotl;
            totalWeight += ndotl;
        }
    }

    prefiltered /= max(totalWeight, 0.001f);
    return float4(prefiltered, 1.0f);
}
