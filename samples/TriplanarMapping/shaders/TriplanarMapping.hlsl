cbuffer PushConstants : register(b0)
{
    float4x4 viewProjection;
    uint modelBufferIndex;
    uint texture0Index;
    uint texture1Index;
    uint samplerIndex;
};

struct VertexInput
{
    float3 position : POSITION;
    float3 color : COLOR0;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float3 color : COLOR0;
    float3 objectPosition : TEXCOORD0;
    float3 objectNormal : NORMAL0;
};

VertexOutput VSMain(VertexInput input, uint instanceId : SV_InstanceID)
{
    StructuredBuffer<float4x4> modelMatrices = ResourceDescriptorHeap[modelBufferIndex];
    const float4x4 model = modelMatrices[instanceId];

    VertexOutput output;
    output.position = mul(viewProjection, mul(model, float4(input.position, 1.0)));
    output.color = input.color;
    output.objectPosition = normalize(input.position);
    output.objectNormal = normalize(input.position);
    return output;
}

float4 Triplanar(uint textureIndex, float3 position, float3 normal)
{
    Texture2D<float4> textureResource = ResourceDescriptorHeap[textureIndex];
    SamplerState textureSampler = SamplerDescriptorHeap[samplerIndex];

    float3 weights = pow(abs(normal), 8.0);
    weights /= max(weights.x + weights.y + weights.z, 0.00001);

    const float4 projectionXY = textureResource.Sample(textureSampler, position.xy);
    const float4 projectionZY = textureResource.Sample(textureSampler, position.zy);
    const float4 projectionXZ = textureResource.Sample(textureSampler, position.xz);

    return projectionXY * weights.z + projectionZY * weights.x + projectionXZ * weights.y;
}

float4 PSMain(VertexOutput input) : SV_Target0
{
    const float3 normal = normalize(input.objectNormal);
    const float4 texture0 = Triplanar(texture0Index, input.objectPosition, normal);
    const float4 texture1 = Triplanar(texture1Index, input.objectPosition, normal);
    return float4(input.color * (texture0.rgb + texture1.rgb), 1.0);
}
