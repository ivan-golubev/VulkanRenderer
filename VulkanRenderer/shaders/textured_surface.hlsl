struct ModelViewProjection
{
    matrix MVP;
};

ConstantBuffer<ModelViewProjection> ModelViewProjectionCB : register(b0);
Texture2D    texture0 : register(t0);
SamplerState sampler0 : register(s0);

struct VSInput
{
    float4 position : POSITION;
    float2 texCoord0 : TEXCOORD0;
};

struct VSOutput
{
    float2 texCoord0 : TEXCOORD0;
	float4 position : SV_Position;
};

VSOutput vs_main(VSInput input)
{
    VSOutput output;
    output.position = mul(ModelViewProjectionCB.MVP, input.position);
    output.texCoord0 = input.texCoord0;
    return output;
}

struct PSInput
{
    float2 texCoord0 : TEXCOORD0;
};

float4 ps_main(PSInput input) : SV_Target
{
    return texture0.Sample(sampler0, input.texCoord0);
}
