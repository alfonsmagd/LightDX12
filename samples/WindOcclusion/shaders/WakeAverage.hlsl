#include "WindCommon.hlsli"

[numthreads(8, 8, 1)]
void CSMain(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (dispatchThreadId.x >= gTextureWidth || dispatchThreadId.y >= gTextureHeight)
    {
        return;
    }

    Texture2D<float4> obstacleMask = ResourceDescriptorHeap[gObstacleMaskIndex];
    Texture2D<uint> rawWakeTexture = ResourceDescriptorHeap[gRawWakeIndex];
    RWTexture2D<uint> filteredWakeTexture = ResourceDescriptorHeap[gFilteredWakeUavIndex];

    const int2 destination = int2(dispatchThreadId.xy);
    const float2 destinationPosition = float2(destination) + 0.5;
    if (SampleSdf(obstacleMask, destinationPosition) <= 0.0)
    {
        filteredWakeTexture[destination] = 0u;
        return;
    }

    uint occlusionSum = 0u;
    uint sampleCount = 0u;
    for (int offsetY = -4; offsetY < 4; ++offsetY)
    {
        for (int offsetX = -4; offsetX < 4; ++offsetX)
        {
            const float2 samplePosition = destinationPosition + float2(offsetX, offsetY);
            if (IsInsideField(samplePosition) && SampleSdf(obstacleMask, samplePosition) > 0.0)
            {
                const uint wakeFactor = rawWakeTexture.Load(int3(int2(samplePosition), 0));
                occlusionSum += 65535u - wakeFactor;
                ++sampleCount;
            }
        }
    }

    const uint averageOcclusion = sampleCount > 0u ? occlusionSum / sampleCount : 0u;
    filteredWakeTexture[destination] = 65535u - averageOcclusion;
}
