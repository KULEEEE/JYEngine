// Split-sum BRDF 적분 LUT (Karis 2013, UE4 IBL).
// 출력 RG = (scale, bias). specular IBL에서 F0 * scale + bias 로 사용한다.
// 환경 무관(roughness, NdotV에만 의존)하므로 1회만 굽고 고정 자산처럼 들고 쓴다.

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
    // clip → uv. x=NdotV, y=roughness. (y down)
    output.UV = float2((p.x + 1.0f) * 0.5f, 1.0f - (p.y + 1.0f) * 0.5f);
    return output;
}

// Hammersley 저불일치 시퀀스
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

float GeometrySchlickGGX(float ndotv, float roughness)
{
    // IBL용 k = a^2 / 2
    float a = roughness;
    float k = (a * a) / 2.0f;
    return ndotv / (ndotv * (1.0f - k) + k);
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    return GeometrySchlickGGX(max(dot(N, L), 0.0f), roughness) * GeometrySchlickGGX(max(dot(N, V), 0.0f), roughness);
}

float2 IntegrateBRDF(float NdotV, float roughness)
{
    NdotV = max(NdotV, 1e-4f);
    float3 V = float3(sqrt(1.0f - NdotV * NdotV), 0.0f, NdotV);
    float3 N = float3(0.0f, 0.0f, 1.0f);

    float A = 0.0f;
    float B = 0.0f;
    const uint SAMPLE_COUNT = 1024u;
    for (uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        float2 Xi = Hammersley(i, SAMPLE_COUNT);
        float3 H = ImportanceSampleGGX(Xi, N, roughness);
        float3 L = normalize(2.0f * dot(V, H) * H - V);

        float ndotl = max(L.z, 0.0f);
        float ndoth = max(H.z, 0.0f);
        float vdoth = max(dot(V, H), 0.0f);

        if (ndotl > 0.0f)
        {
            float G = GeometrySmith(N, V, L, roughness);
            float Gvis = (G * vdoth) / (ndoth * NdotV);
            float Fc = pow(1.0f - vdoth, 5.0f);
            A += (1.0f - Fc) * Gvis;
            B += Fc * Gvis;
        }
    }
    return float2(A, B) / float(SAMPLE_COUNT);
}

float2 pMain(VSOut input) : SV_TARGET
{
    // UV.x = NdotV, UV.y = roughness
    return IntegrateBRDF(input.UV.x, max(input.UV.y, 0.02f));
}
