struct ModelViewProjection
{
    matrix MVP;
};

ConstantBuffer<ModelViewProjection> ModelViewProjectionCB : register(b0);
Texture2D    texture1 : register(t1);
SamplerState sampler1 : register(s1);

struct VSInput
{
    float4 position : POSITION;
    float2 texCoord : TEXCOORD;
};

struct VSOutput
{
    float2 texCoord : TEXCOORD;
	float4 position : SV_Position;
};

VSOutput vs_main(VSInput input)
{
    VSOutput output;
    output.position = mul(ModelViewProjectionCB.MVP, input.position);
    output.texCoord = input.texCoord;
    return output;
}

struct PSInput
{
    float2 texCoord : TEXCOORD;
};

float4 ps_main(PSInput input) : SV_Target
{
    return texture1.Sample(sampler1, input.texCoord);
}
