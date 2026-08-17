cbuffer SpritePushConstants : register(b0)
{
    float4 gRect;
    float4 gUvRect;
    float4 gTint;
    float2 gLogicalViewport;
    uint gTextureIndex;
    float gRotation;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

VertexOutput VSMain(uint vertexId : SV_VertexID)
{
    static const float2 corners[6] =
    {
        float2(0.0, 0.0),
        float2(1.0, 0.0),
        float2(0.0, 1.0),
        float2(0.0, 1.0),
        float2(1.0, 0.0),
        float2(1.0, 1.0)
    };

    const float2 corner = corners[vertexId];
    const float2 localPosition = (corner - 0.5) * gRect.zw;
    const float sine = sin(gRotation);
    const float cosine = cos(gRotation);
    const float2 rotatedPosition = float2(
        localPosition.x * cosine - localPosition.y * sine,
        localPosition.x * sine + localPosition.y * cosine);
    const float2 pixelPosition = gRect.xy + gRect.zw * 0.5 + rotatedPosition;
    const float2 ndc = float2(
        pixelPosition.x / gLogicalViewport.x * 2.0 - 1.0,
        1.0 - pixelPosition.y / gLogicalViewport.y * 2.0);

    VertexOutput output;
    output.position = float4(ndc, 0.0, 1.0);
    output.uv = lerp(gUvRect.xy, gUvRect.zw, corner);
    return output;
}

float4 PSMain(VertexOutput input) : SV_Target0
{
    Texture2D<float4> sprite = ResourceDescriptorHeap[gTextureIndex];
    uint width = 0;
    uint height = 0;
    sprite.GetDimensions(width, height);

    const float2 dimensions = float2(width, height);
    const float2 halfTexel = 0.5 / dimensions;
    const float2 uvMinimum = min(gUvRect.xy, gUvRect.zw) + halfTexel;
    const float2 uvMaximum = max(gUvRect.xy, gUvRect.zw) - halfTexel;
    const float2 sampleUv = clamp(input.uv, uvMinimum, uvMaximum);
    const uint2 texel = min(uint2(sampleUv * dimensions), uint2(width - 1, height - 1));
    const float4 color = sprite.Load(int3(texel, 0)) * gTint;
    clip(color.a - 0.02);
    return color;
}
