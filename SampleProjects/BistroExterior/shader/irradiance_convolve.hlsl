// 캡처된 환경 큐브를 코사인 가중 적분해 diffuse irradiance 큐브를 만든다.
// 면별로 full-screen 1패스. 출력 texel의 world 방향을 face basis로 복원해 반구를 적분한다.
// irradiance는 저주파라 해상도/샘플이 작아도 충분하다.

cbuffer PerFace : register(b0)
{
    float4 FaceForward; // 면 중심 방향
    float4 FaceRight;   // +u 방향
    float4 FaceUp;      // +v 방향
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

float4 pMain(VSOut input) : SV_TARGET
{
    // 출력 texel → world 방향 (face basis, ndc는 y up)
    float2 ndc = float2(input.UV.x * 2.0f - 1.0f, 1.0f - input.UV.y * 2.0f);
    float3 N = normalize(FaceForward.xyz + ndc.x * FaceRight.xyz + ndc.y * FaceUp.xyz);

    // N 기준 탄젠트 프레임
    float3 up = abs(N.y) < 0.999f ? float3(0.0f, 1.0f, 0.0f) : float3(0.0f, 0.0f, 1.0f);
    float3 right = normalize(cross(up, N));
    up = cross(N, right);

    float3 irradiance = float3(0.0f, 0.0f, 0.0f);
    float sampleCount = 0.0f;

    const float deltaPhi = 0.05f;
    const float deltaTheta = 0.05f;
    for (float phi = 0.0f; phi < 2.0f * PI; phi += deltaPhi)
    {
        for (float theta = 0.0f; theta < 0.5f * PI; theta += deltaTheta)
        {
            // 탄젠트 공간 → world
            float3 tangentSample = float3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            float3 worldSample = tangentSample.x * right + tangentSample.y * up + tangentSample.z * N;

            irradiance += EnvCube.SampleLevel(EnvSampler, worldSample, 0).rgb * cos(theta) * sin(theta);
            sampleCount += 1.0f;
        }
    }

    irradiance = PI * irradiance / max(sampleCount, 1.0f);
    return float4(irradiance, 1.0f);
}
