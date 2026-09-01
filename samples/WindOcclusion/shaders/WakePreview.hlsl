#include "WindCommon.hlsli"

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

VertexOutput VSMain(uint vertexId : SV_VertexID)
{
    const float2 positions[3] =
    {
        float2(-1.0, -1.0),
        float2(-1.0, 3.0),
        float2(3.0, -1.0)
    };

    VertexOutput output;
    output.position = float4(positions[vertexId], 0.0, 1.0);
    output.uv = float2((positions[vertexId].x + 1.0) * 0.5, (1.0 - positions[vertexId].y) * 0.5);
    return output;
}

float4 PSMain(VertexOutput input) : SV_Target0
{
    Texture2D<float4> obstacleMask = ResourceDescriptorHeap[gObstacleMaskIndex];
    Texture2D<uint> wakeTexture = ResourceDescriptorHeap[gFilteredWakeIndex];

    const int2 pixel = int2(saturate(input.uv) * float2(gTextureWidth - 1u, gTextureHeight - 1u));
    if (SampleSdf(obstacleMask, float2(pixel)) <= 0.0)
    {
        return float4(0.18, 0.02, 0.02, 1.0);
    }

    const float calculatedWakeFactor = DecodeWakeFactor(wakeTexture.Load(int3(pixel, 0)));
    const float wakeFactor = lerp(1.0, calculatedWakeFactor, gWakeEffect);
    const float3 wakeColor = lerp(float3(0.02, 0.03, 0.06), float3(0.95, 0.95, 0.95), wakeFactor);
    return float4(wakeColor, 1.0);
}
