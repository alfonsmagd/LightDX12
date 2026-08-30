#include "TransparencyCommon.hlsli"

struct VertexInput
{
    float3 position : POSITION;
};

float4 VSMain(VertexInput input) : SV_Position
{
    const float4 worldPosition = mul(model, float4(input.position, 1.0));
    return mul(viewProjection, worldPosition);
}

float4 PSMain() : SV_Target0
{
    return color;
}
