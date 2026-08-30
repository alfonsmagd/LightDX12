cbuffer PushConstants : register(b0)
{
    float4x4 viewProjection;
    float4x4 skyViewProjection;
    float4x4 model;
    float4 color;
    uint cubeMapIndex;
    uint samplerIndex;
};
