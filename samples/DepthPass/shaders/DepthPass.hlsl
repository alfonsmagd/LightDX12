cbuffer PushConstants : register(b0)
{
    float4x4 modelViewProjection;
    float4 color;
};

struct VertexInput
{
    float3 position : POSITION;
};

struct VertexOutput
{
    float4 position : SV_Position;
};

VertexOutput VSMain(VertexInput input)
{
    VertexOutput output;
    output.position = mul(modelViewProjection, float4(input.position, 1.0));
    return output;
}

void PSDepth()
{
}

float4 PSColor() : SV_Target0
{
    return color;
}
