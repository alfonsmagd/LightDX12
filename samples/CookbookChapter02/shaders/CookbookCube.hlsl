cbuffer PushConstants : register(b0)
{
    float4x4 mvp;
    uint textureIndex;
    uint samplerIndex;
};

struct VSOutput
{
    float4 position : SV_Position;
    float3 color : COLOR0;
    float2 uv : TEXCOORD0;
    nointerpolation uint wireframe : TEXCOORD1;
};

static const float3 positions[8] =
{
    float3(-1.0, -1.0,  1.0), float3( 1.0, -1.0,  1.0),
    float3( 1.0,  1.0,  1.0), float3(-1.0,  1.0,  1.0),
    float3(-1.0, -1.0, -1.0), float3( 1.0, -1.0, -1.0),
    float3( 1.0,  1.0, -1.0), float3(-1.0,  1.0, -1.0)
};

static const float3 colors[8] =
{
    float3(1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0),
    float3(0.0, 0.0, 1.0), float3(1.0, 1.0, 0.0),
    float3(1.0, 1.0, 0.0), float3(0.0, 0.0, 1.0),
    float3(0.0, 1.0, 0.0), float3(1.0, 0.0, 0.0)
};

static const uint indices[36] =
{
    0, 1, 2, 2, 3, 0,
    1, 5, 6, 6, 2, 1,
    7, 6, 5, 5, 4, 7,
    4, 0, 3, 3, 7, 4,
    4, 5, 1, 1, 0, 4,
    3, 2, 6, 6, 7, 3
};

VSOutput BuildCubeVertex(uint vertexID, bool wireframe)
{
    const uint index = indices[vertexID];

    VSOutput output;
    output.position = mul(mvp, float4(positions[index], 1.0));
    output.color = colors[index];
    output.wireframe = wireframe ? 1u : 0u;

    const float3 localPosition = positions[index];
    const uint faceIndex = vertexID / 6;
    if (faceIndex == 0 || faceIndex == 2)
    {
        const float direction = faceIndex == 0 ? -1.0 : 1.0;
        output.uv = float2(direction * localPosition.x, -localPosition.y) * 0.5 + 0.5;
    }
    else if (faceIndex == 1 || faceIndex == 3)
    {
        const float direction = faceIndex == 1 ? 1.0 : -1.0;
        output.uv = float2(direction * localPosition.z, -localPosition.y) * 0.5 + 0.5;
    }
    else
    {
        const float direction = faceIndex == 4 ? 1.0 : -1.0;
        output.uv = float2(localPosition.x, direction * localPosition.z) * 0.5 + 0.5;
    }
    return output;
}

VSOutput VSMainSolid(uint vertexID : SV_VertexID)
{
    return BuildCubeVertex(vertexID, false);
}

VSOutput VSMainWireframe(uint vertexID : SV_VertexID)
{
    return BuildCubeVertex(vertexID, true);
}

float4 PSMain(VSOutput input) : SV_Target0
{
    if (input.wireframe != 0)
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }

    Texture2D<float4> logo = ResourceDescriptorHeap[textureIndex];
    SamplerState logoSampler = SamplerDescriptorHeap[samplerIndex];
    const float3 surfaceColor = logo.Sample(logoSampler, input.uv).rgb * (0.5 + 0.5 * input.color);

    return float4(surfaceColor, 1.0);
}
