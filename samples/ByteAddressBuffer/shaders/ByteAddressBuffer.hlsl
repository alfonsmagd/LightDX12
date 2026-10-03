#include "Ldx12_Defines.hlsli"

struct SceneConstants
{
    float4x4 modelViewProjection;
    float4x4 model;
    float4 lightColor;
    float4 lightDirection;
};

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    uint colorByteOffset : TEXCOORD0;
};

struct VSOutput
{
    float4 position : SV_Position;
    float3 normal : NORMAL0;
    nointerpolation float4 color : COLOR0;
};

VSOutput VSMain(VSInput input)
{
    ConstantBuffer<SceneConstants> scene = ResourceDescriptorHeap[LDX12_CBV_SLOT_FREECB0];
    ByteAddressBuffer faceColors = ResourceDescriptorHeap[LDX12_SRV_SLOT_FREESRV0];

    VSOutput output;
    output.position = mul(scene.modelViewProjection, float4(input.position, 1.0));
    output.normal = mul((float3x3)scene.model, input.normal);
    output.color = asfloat(faceColors.Load4(input.colorByteOffset));

    return output;
}

float4 PSMain(VSOutput input) : SV_Target0
{
    ConstantBuffer<SceneConstants> scene = ResourceDescriptorHeap[LDX12_CBV_SLOT_FREECB0];

    const float lighting = saturate(dot(normalize(input.normal), scene.lightDirection.xyz)) * 0.72 + 0.28;

    return float4(input.color.rgb * scene.lightColor.rgb * lighting, input.color.a);
}
