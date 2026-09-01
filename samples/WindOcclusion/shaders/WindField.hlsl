#include "WindCommon.hlsli"

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

float DistanceToSegment(float2 samplePosition, float2 start, float2 end)
{
    const float2 segment = end - start;
    const float segmentLengthSquared = max(dot(segment, segment), 1e-5);
    const float projection = saturate(dot(samplePosition - start, segment) / segmentLengthSquared);
    return length(samplePosition - (start + segment * projection));
}

VertexOutput VSMain(uint vertexId : SV_VertexID)
{
    const float2 textureCoordinates[6] =
    {
        float2(0.0, 0.0),
        float2(1.0, 0.0),
        float2(0.0, 1.0),
        float2(0.0, 1.0),
        float2(1.0, 0.0),
        float2(1.0, 1.0)
    };

    VertexOutput output;
    output.uv = textureCoordinates[vertexId];
    const float3 worldPosition = float3(
        (output.uv.x - 0.5) * gFieldWorldSize,
        0.0,
        (0.5 - output.uv.y) * gFieldWorldSize);
    output.position = mul(gModelViewProjection, float4(worldPosition, 1.0));
    return output;
}

float4 PSMain(VertexOutput input) : SV_Target0
{
    Texture2D<float4> inputWindTexture = ResourceDescriptorHeap[gInputWindIndex];
    Texture2D<float4> obstacleMask = ResourceDescriptorHeap[gObstacleMaskIndex];
    Texture2D<uint> wakeTexture = ResourceDescriptorHeap[gFilteredWakeIndex];
    SamplerState linearClampSampler = SamplerDescriptorHeap[0];

    const float2 gridSize = float2(28.0, 28.0);
    const float2 cell = floor(input.uv * gridSize);
    const float2 cellUv = (cell + 0.5) / gridSize;
    const float2 cellPosition = cellUv * float2(gTextureWidth, gTextureHeight);
    const float2 inputWind = LoadWind(inputWindTexture, cellPosition);
    float wakeFactor = 1.0;
    if (gVisualizeInputWind == 0u)
    {
        const float calculatedWakeFactor = DecodeWakeFactor(wakeTexture.Load(int3(int2(cellPosition), 0)));
        wakeFactor = lerp(1.0, calculatedWakeFactor, gWakeEffect);
    }

    const float2 finalWind = inputWind * wakeFactor;
    const float strength = saturate(length(finalWind));
    const float2 direction = strength > 1e-4 ? normalize(finalWind) : float2(1.0, 0.0);
    const float2 localPosition = frac(input.uv * gridSize) - 0.5;
    const float arrowLength = lerp(0.04, 0.64, strength);
    const float2 arrowStart = -direction * arrowLength * 0.5;
    const float2 arrowTip = direction * arrowLength * 0.5;
    const float2 perpendicular = float2(-direction.y, direction.x);
    const float2 headA = arrowTip - direction * 0.13 + perpendicular * 0.10;
    const float2 headB = arrowTip - direction * 0.13 - perpendicular * 0.10;

    float arrowDistance = DistanceToSegment(localPosition, arrowStart, arrowTip);
    arrowDistance = min(arrowDistance, DistanceToSegment(localPosition, arrowTip, headA));
    arrowDistance = min(arrowDistance, DistanceToSegment(localPosition, arrowTip, headB));
    float arrow = 1.0 - smoothstep(0.025, 0.055, arrowDistance);
    arrow *= step(0.035, strength);

    const float obstacle = obstacleMask.SampleLevel(linearClampSampler, input.uv, 0.0).r;
    if (obstacle < 0.5)
    {
        return float4(0.055, 0.06, 0.07, 1.0);
    }

    const float3 background = lerp(float3(0.012, 0.022, 0.035), float3(0.025, 0.105, 0.145), strength * 0.75);
    const float directionColor = direction.y * 0.5 + 0.5;
    const float3 arrowColor = lerp(float3(0.20, 0.82, 1.0), float3(1.0, 0.78, 0.20), directionColor);
    return float4(lerp(background, arrowColor, arrow * lerp(0.25, 1.0, strength)), 1.0);
}
