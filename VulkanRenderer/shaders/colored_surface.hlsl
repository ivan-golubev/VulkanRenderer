struct ModelViewProjection
{
    matrix MVP;
};

ConstantBuffer<ModelViewProjection> ModelViewProjectionCB : register(b0);

struct VSInput
{
    float4 position : POSITION;
    float4 color : COLOR;
};

struct VSOutput
{
    float4 color : COLOR;
	float4 position : SV_Position;
};

VSOutput vs_main(VSInput input)
{
    VSOutput output;
    output.position = mul(ModelViewProjectionCB.MVP, input.position);
    output.color = input.color;
    return output;
}

struct PSInput
{
    float4 color : COLOR;
};

float4 ps_main(PSInput input) : SV_Target
{
    return input.color;
}
