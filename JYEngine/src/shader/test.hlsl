// Vertex Shader (VertexShader.hlsl)
struct VS_INPUT {
    float4 Pos : POSITION; // 입력 정점 위치
    float4 Color : COLOR;  // 입력 색상
};

struct PS_INPUT {
    float4 Pos : SV_POSITION; // 변환된 정점 위치
    float4 Color : COLOR;     // 정점 색상
};

PS_INPUT vMain(VS_INPUT input) {
    PS_INPUT output;
    output.Pos = input.Pos; // 위치를 변환 (현재는 단순히 위치 그대로)
    output.Color = input.Color;          // 색상 그대로 전달
    return output;
}

float4 pMain(PS_INPUT input) : SV_TARGET {
    return input.Color; // 정점 색상을 그대로 픽셀 색상으로 반환
}