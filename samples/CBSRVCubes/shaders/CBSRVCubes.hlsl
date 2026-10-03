#include "Ldx12_Defines.hlsli"

#define SAMPLE_SCENE_CBV_SLOT LDX12_CBV_SLOT_FREECB0
#define SAMPLE_CUBES_SRV_SLOT LDX12_SRV_SLOT_FREESRV0

struct MatrixRows
{
    float4 row0;
    float4 row1;
    float4 row2;
    float4 row3;
};

struct SceneConstants
{
    float aspectRatio;
    float viewDistance;
    uint textureIndex;
    uint samplerIndex;
    float4 lightDirection;
};

struct CubeData
{
    MatrixRows model;
    float4 color;
};

struct VSOutput
{
    float4 position : SV_Position;
    float3 normal : NORMAL0;
    float2 uv : TEXCOORD0;
    nointerpolation float3 color : COLOR0;
};

float4 TransformPoint(MatrixRows matrix, float3 localPosition)
{
    const float4 value = float4(localPosition, 1.0);
    return float4(
        dot(matrix.row0, value),
        dot(matrix.row1, value),
        dot(matrix.row2, value),
        dot(matrix.row3, value));
}

float3 TransformNormal(MatrixRows matrix, float3 localNormal)
{
    return normalize(float3(
        dot(matrix.row0.xyz, localNormal),
        dot(matrix.row1.xyz, localNormal),
        dot(matrix.row2.xyz, localNormal)));
}

VSOutput VSMain(uint vertexID : SV_VertexID, uint instanceID : SV_InstanceID)
{
    // Clockwise front faces with outward-facing normals.
    static const float3 positions[36] =
    {
        float3(-1.0, -1.0, -1.0), float3( 1.0,  1.0, -1.0), float3( 1.0, -1.0, -1.0),
        float3(-1.0, -1.0, -1.0), float3(-1.0,  1.0, -1.0), float3( 1.0,  1.0, -1.0),

        float3(-1.0, -1.0,  1.0), float3( 1.0, -1.0,  1.0), float3( 1.0,  1.0,  1.0),
        float3(-1.0, -1.0,  1.0), float3( 1.0,  1.0,  1.0), float3(-1.0,  1.0,  1.0),

        float3(-1.0, -1.0,  1.0), float3( 1.0, -1.0, -1.0), float3( 1.0, -1.0,  1.0),
        float3(-1.0, -1.0,  1.0), float3(-1.0, -1.0, -1.0), float3( 1.0, -1.0, -1.0),

        float3(-1.0,  1.0, -1.0), float3( 1.0,  1.0,  1.0), float3( 1.0,  1.0, -1.0),
        float3(-1.0,  1.0, -1.0), float3(-1.0,  1.0,  1.0), float3( 1.0,  1.0,  1.0),

        float3( 1.0, -1.0, -1.0), float3( 1.0,  1.0,  1.0), float3( 1.0, -1.0,  1.0),
        float3( 1.0, -1.0, -1.0), float3( 1.0,  1.0, -1.0), float3( 1.0,  1.0,  1.0),

        float3(-1.0, -1.0,  1.0), float3(-1.0,  1.0, -1.0), float3(-1.0, -1.0, -1.0),
        float3(-1.0, -1.0,  1.0), float3(-1.0,  1.0,  1.0), float3(-1.0,  1.0, -1.0)
    };

    static const float3 normals[36] =
    {
        float3( 0.0,  0.0, -1.0), float3( 0.0,  0.0, -1.0), float3( 0.0,  0.0, -1.0),
        float3( 0.0,  0.0, -1.0), float3( 0.0,  0.0, -1.0), float3( 0.0,  0.0, -1.0),

        float3( 0.0,  0.0,  1.0), float3( 0.0,  0.0,  1.0), float3( 0.0,  0.0,  1.0),
        float3( 0.0,  0.0,  1.0), float3( 0.0,  0.0,  1.0), float3( 0.0,  0.0,  1.0),

        float3( 0.0, -1.0,  0.0), float3( 0.0, -1.0,  0.0), float3( 0.0, -1.0,  0.0),
        float3( 0.0, -1.0,  0.0), float3( 0.0, -1.0,  0.0), float3( 0.0, -1.0,  0.0),

        float3( 0.0,  1.0,  0.0), float3( 0.0,  1.0,  0.0), float3( 0.0,  1.0,  0.0),
        float3( 0.0,  1.0,  0.0), float3( 0.0,  1.0,  0.0), float3( 0.0,  1.0,  0.0),

        float3( 1.0,  0.0,  0.0), float3( 1.0,  0.0,  0.0), float3( 1.0,  0.0,  0.0),
        float3( 1.0,  0.0,  0.0), float3( 1.0,  0.0,  0.0), float3( 1.0,  0.0,  0.0),

        float3(-1.0,  0.0,  0.0), float3(-1.0,  0.0,  0.0), float3(-1.0,  0.0,  0.0),
        float3(-1.0,  0.0,  0.0), float3(-1.0,  0.0,  0.0), float3(-1.0,  0.0,  0.0)
    };

    ConstantBuffer<SceneConstants> scene = ResourceDescriptorHeap[SAMPLE_SCENE_CBV_SLOT];
    StructuredBuffer<CubeData> cubes = ResourceDescriptorHeap[SAMPLE_CUBES_SRV_SLOT];
    const CubeData cube = cubes[instanceID];

    float3 worldPosition = TransformPoint(cube.model, positions[vertexID]).xyz;
    worldPosition.z += scene.viewDistance;

    const float perspectiveScale = 2.25 / worldPosition.z;
    const float2 clipXY = float2(
        worldPosition.x * perspectiveScale / scene.aspectRatio,
        worldPosition.y * perspectiveScale);
    const float clipZ = (worldPosition.z - 1.0) / (scene.viewDistance + 18.0);

    VSOutput output;
    output.position = float4(clipXY, clipZ, 1.0);
    output.normal = TransformNormal(cube.model, normals[vertexID]);
    output.color = cube.color.rgb;

    const float3 localPosition = positions[vertexID];
    const float3 localNormal = normals[vertexID];
    if (abs(localNormal.z) > 0.5)
    {
        output.uv = float2(-localNormal.z * localPosition.x, -localPosition.y) * 0.5 + 0.5;
    }
    else if (abs(localNormal.x) > 0.5)
    {
        output.uv = float2(localNormal.x * localPosition.z, -localPosition.y) * 0.5 + 0.5;
    }
    else
    {
        output.uv = float2(localPosition.x, -localNormal.y * localPosition.z) * 0.5 + 0.5;
    }
    return output;
}

float4 PSMain(VSOutput input) : SV_Target0
{
    ConstantBuffer<SceneConstants> scene = ResourceDescriptorHeap[SAMPLE_SCENE_CBV_SLOT];
    const float3 lightDirection = normalize(scene.lightDirection.xyz);
    const float lighting = saturate(dot(normalize(input.normal), lightDirection)) * 0.72 + 0.28;
    Texture2D<float4> logo = ResourceDescriptorHeap[scene.textureIndex];
    SamplerState logoSampler = SamplerDescriptorHeap[scene.samplerIndex];
    const float3 surfaceColor = logo.Sample(logoSampler, input.uv).rgb * (0.5 + 0.5 * input.color);

    return float4(surfaceColor * lighting, 1.0);
}
