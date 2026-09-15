#ifndef WIND_COMMON_HLSLI
#define WIND_COMMON_HLSLI

cbuffer PushConstants : register(b0)
{
    float4x4 gModelViewProjection;
    float4x4 gModel;
    float4 gColor;
    uint gInputWindIndex;
    uint gObstacleMaskIndex;
    uint gRawWakeIndex;
    uint gRawWakeUavIndex;
    uint gFilteredWakeIndex;
    uint gFilteredWakeUavIndex;
    uint gTextureWidth;
    uint gTextureHeight;
    uint gMaxWakeSteps;
    float gFieldWorldSize;
    float gEdgeStep;
    float gPropagationStep;
    float gWakeLength;
    uint gVisualizeInputWind;
    float gWakeEffect;
};

bool IsInsideField(float2 position)
{
    return all(position >= 0.0) && position.x < float(gTextureWidth) && position.y < float(gTextureHeight);
}

float2 LoadWind(Texture2D<float4> windTexture, float2 position)
{
    return windTexture.Load(int3(int2(position), 0)).xy;
}

float SampleSdf(Texture2D<float4> obstacleMask, float2 position)
{
    return obstacleMask.Load(int3(int2(position), 0)).r > 0.5 ? 1.0 : -1.0;
}

float DecodeWakeFactor(uint encodedFactor)
{
    return float(encodedFactor) / 65535.0;
}

#endif
