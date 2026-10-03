#include "CubeMapCommon.hlsli"

struct VertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD0;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float3 worldPosition : TEXCOORD0;
    float3 worldNormal : TEXCOORD1;
    float2 uv : TEXCOORD2;
};

VertexOutput VSMain(VertexInput input)
{
    VertexOutput output;
    const float4 worldPosition = mul(model, float4(input.position, 1.0));
    output.position = mul(viewProjection, worldPosition);
    output.worldPosition = worldPosition.xyz;
    output.uv = input.uv;
    output.worldNormal = normalize(mul((float3x3)model, input.normal));
    return output;
}

float4 PSMain(VertexOutput input) : SV_Target0
{
    TextureCube<float4> environment = ResourceDescriptorHeap[cubeMapIndex];
    SamplerState environmentSampler = SamplerDescriptorHeap[samplerIndex];
    const float3 normal = normalize(input.worldNormal);
    const float3 viewDirection = normalize(input.worldPosition - cameraPosition.xyz);
    const float3 reflectionDirection = reflect(viewDirection, normal);
    const float3 reflection = environment.Sample(environmentSampler, reflectionDirection).rgb;
    if (useLogo != 0)
    {
        Texture2D<float4> logo = ResourceDescriptorHeap[textureIndex];
        const float3 surfaceColor = logo.Sample(environmentSampler, input.uv).rgb;

        return float4(surfaceColor * (0.5 + 0.5 * reflection), 1.0);
    }

    return float4(reflection, 1.0);
}
