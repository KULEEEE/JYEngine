cbuffer PerFrame : register(b0)
{
    matrix ViewProjection;
}

cbuffer PerObject : register(b2)
{
    matrix World;
}

struct VS_INPUT
{
    float4 Pos : POSITION;
};

struct PS_INPUT
{
    float4 Pos : SV_POSITION;
    float3 WorldPos : TEXCOORD0;
};

PS_INPUT vMain(VS_INPUT input)
{
    PS_INPUT output;
    float4 worldPos = mul(input.Pos, World);
    output.WorldPos = worldPos.xyz;
    output.Pos = mul(worldPos, ViewProjection);
    return output;
}

float gridLine(float2 coord, float scale, float thickness)
{
    float2 scaled = coord / scale;
    float2 cell = abs(frac(scaled - 0.5f) - 0.5f) / fwidth(scaled);
    float gridDistance = min(cell.x, cell.y);
    return 1.0f - saturate(gridDistance - thickness);
}

float4 pMain(PS_INPUT input) : SV_TARGET
{
    float2 coord = input.WorldPos.xz;
    float minor = gridLine(coord, 1.0f, 0.0f);
    float major = gridLine(coord, 10.0f, 0.0f);

    float minorAlpha = saturate(minor) * 0.10f;
    float majorAlpha = saturate(major) * 0.24f;
    float axisX = saturate(1.0f - abs(coord.x) / max(fwidth(coord.x), 0.0001f));
    float axisZ = saturate(1.0f - abs(coord.y) / max(fwidth(coord.y), 0.0001f));
    float axisAlpha = max(axisX, axisZ) * 0.32f;

    float alpha = saturate(max(max(minorAlpha, majorAlpha), axisAlpha));
    return float4(1.0f, 1.0f, 1.0f, alpha);
}
