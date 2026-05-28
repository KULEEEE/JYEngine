cbuffer MyConstantBuffer : register(b0)
{
    float4x4 modelViewProjection;
};

cbuffer Test : register(b1)
{
    float4x4 test;
}

Texture2D myTexture : register(t0);
SamplerState mySampler : register(s0);

Texture2D myTexture2 : register(t1);

// Vertex Shader (VertexShader.hlsl)
struct VS_INPUT {
    float4 Pos : POSITION; // 입력 정점 위치
    //float2 Uvs : TEXCOORD0;  // 입력 색상
};

struct PS_INPUT {
    float4 Pos : SV_POSITION; // 변환된 정점 위치
    //float2 Uvs : TEXCOORD0; // 정점 색상
};

PS_INPUT vMain(VS_INPUT input) {
    PS_INPUT output;

    float4 position;
    position = mul(modelViewProjection, input.Pos); // 위치를 변환 (현재는 단순히 위치 그대로)
    output.Pos = mul(test, position);
    //output.Uvs = input.Uvs;
    return output;
}

float4 pMain(PS_INPUT input) : SV_TARGET 
{
    float2 uv = float2(0.f,0.f);
    float4 color = myTexture.Sample(mySampler, uv);
    return color;
}