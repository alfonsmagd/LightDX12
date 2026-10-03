cbuffer PushConstants : register(b0)
{
    float4x4 modelViewProjection;
    float4 color;
    uint textureIndex;
    uint samplerIndex;
};

struct VertexInput
{
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

VertexOutput VSMain(VertexInput input)
{
    VertexOutput output;
    output.position = mul(modelViewProjection, float4(input.position, 1.0));
    // Match the logo orientation to the right-handed camera.
    output.uv = float2(1.0 - input.uv.x, input.uv.y);
    return output;
}

void PSDepth()
{
}

float4 PSColor(VertexOutput input) : SV_Target0
{
    Texture2D<float4> logo = ResourceDescriptorHeap[textureIndex];
    SamplerState logoSampler = SamplerDescriptorHeap[samplerIndex];
    const float3 surfaceColor = logo.Sample(logoSampler, input.uv).rgb * (0.5 + 0.5 * color.rgb);

    return float4(surfaceColor, color.a);
}
