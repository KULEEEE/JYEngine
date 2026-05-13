Texture2D GBufferAlbedo : register(t0);

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

float4 pMain(VS_OUTPUT input) : SV_TARGET
{
    int2 pixel = int2(input.Pos.xy);
    return GBufferAlbedo.Load(int3(pixel, 0));
}
