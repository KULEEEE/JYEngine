struct VSInput
{
    float4 position : POSITION;
};

struct VSOutput
{
    float4 position : SV_POSITION;
};

VSOutput vMain(VSInput input)
{
    VSOutput output;
    output.position = input.position;
    return output;
}

float4 pMain(VSOutput input) : SV_TARGET
{
    return float4(0.70f, 1.0f, 0.48f, 0.92f);
}
