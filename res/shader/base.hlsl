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

    output.Pos = input.Pos;
    //output.Uvs = input.Uvs;
    return output;
}

float4 pMain(PS_INPUT input) : SV_TARGET 
{
    return float4(1,1,1,1);
}