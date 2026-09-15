#include "WindCommon.hlsli"

struct VertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float3 normal : NORMAL;
};

VertexOutput VSMain(VertexInput input)
{
    VertexOutput output;
    output.position = mul(gModelViewProjection, float4(input.position, 1.0));
    output.normal = normalize(mul((float3x3)gModel, input.normal));
    return output;
}

float4 PSMain(VertexOutput input) : SV_Target0
{
    const float3 lightDirection = normalize(float3(-0.45, 0.85, -0.30));
    const float lighting = 0.28 + saturate(dot(normalize(input.normal), lightDirection)) * 0.72;
    return float4(gColor.rgb * lighting, 1.0);
}
