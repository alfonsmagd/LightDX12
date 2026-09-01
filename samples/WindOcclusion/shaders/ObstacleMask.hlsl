#include "WindCommon.hlsli"

float4 VSMain(float3 position : POSITION) : SV_Position
{
    return mul(gModelViewProjection, float4(position, 1.0));
}

float4 PSMain() : SV_Target0
{
    return float4(0.0, 0.0, 0.0, 1.0);
}
