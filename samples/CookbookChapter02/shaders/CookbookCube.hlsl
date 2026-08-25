cbuffer PushConstants : register(b0)
{
    float4x4 mvp;
};

struct VSOutput
{
    float4 position : SV_Position;
    float3 color : COLOR0;
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
    output.color = wireframe ? float3(0.0, 0.0, 0.0) : colors[index];
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
    return float4(input.color, 1.0);
}
