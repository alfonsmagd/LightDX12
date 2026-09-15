#include "WindCommon.hlsli"

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (dispatchThreadId.x >= gTextureWidth || dispatchThreadId.y >= gTextureHeight)
    {
        return;
    }

    Texture2D<float4> inputWindTexture = ResourceDescriptorHeap[gInputWindIndex];
    Texture2D<float4> obstacleMask = ResourceDescriptorHeap[gObstacleMaskIndex];
    RWTexture2D<uint> rawWakeTexture = ResourceDescriptorHeap[gRawWakeUavIndex];

    const float2 seedPosition = float2(dispatchThreadId.xy) + 0.5;
    if (SampleSdf(obstacleMask, seedPosition) <= 0.0)
    {
        return;
    }

    const float2 seedWind = LoadWind(inputWindTexture, seedPosition);
    const float seedWindLength = length(seedWind);
    if (seedWindLength <= 1e-5)
    {
        return;
    }

    const float2 seedDirection = seedWind / seedWindLength;
    const float2 previousPosition = seedPosition - seedDirection * gEdgeStep;
    if (!IsInsideField(previousPosition) || SampleSdf(obstacleMask, previousPosition) > 0.0)
    {
        return;
    }

    float2 position = seedPosition;
    float travelledDistance = 0.0;
    for (uint stepIndex = 0u; stepIndex < gMaxWakeSteps && travelledDistance < gWakeLength; ++stepIndex)
    {
        if (!IsInsideField(position) || SampleSdf(obstacleMask, position) <= 0.0)
        {
            break;
        }

        const float wakeFactor = smoothstep(0.0, gWakeLength, travelledDistance);
        const uint encodedWakeFactor = (uint)round(saturate(wakeFactor) * 65535.0);
        const int2 destination = int2(position);
        uint previousWakeFactor;
        InterlockedMin(rawWakeTexture[destination], encodedWakeFactor, previousWakeFactor);

        const float2 localWind = LoadWind(inputWindTexture, position);
        const float localWindLength = length(localWind);
        if (localWindLength <= 1e-5)
        {
            break;
        }

        position += (localWind / localWindLength) * gPropagationStep;
        travelledDistance += gPropagationStep;
    }
}
