cbuffer SkyboxConstants : register( b0 )
{
	float4x4 viewProjection;
	uint cubeMapIndex;
	uint samplerIndex;
	uint2 padding;
};

struct VertexOutput
{
	float4 position : SV_Position;
	float3 direction : TEXCOORD0;
};

static const float3 cubePositions[ 8 ] =
{
	float3( -1.0, -1.0, 1.0 ),
	float3( 1.0, -1.0, 1.0 ),
	float3( 1.0, 1.0, 1.0 ),
	float3( -1.0, 1.0, 1.0 ),
	float3( -1.0, -1.0, -1.0 ),
	float3( 1.0, -1.0, -1.0 ),
	float3( 1.0, 1.0, -1.0 ),
	float3( -1.0, 1.0, -1.0 )
};

static const uint cubeIndices[ 36 ] =
{
	0, 1, 2, 2, 3, 0,
	1, 5, 6, 6, 2, 1,
	7, 6, 5, 5, 4, 7,
	4, 0, 3, 3, 7, 4,
	4, 5, 1, 1, 0, 4,
	3, 2, 6, 6, 7, 3
};

VertexOutput VSMain( uint vertexId : SV_VertexID )
{
	VertexOutput output;
	output.direction = cubePositions[ cubeIndices[ vertexId ] ];
	output.position = mul( viewProjection, float4( output.direction, 1.0 ) );
	output.position.z = output.position.w;

	return output;
}

float4 PSMain( VertexOutput input ) : SV_Target0
{
	TextureCube<float4> skybox = ResourceDescriptorHeap[ cubeMapIndex ];
	SamplerState skyboxSampler = SamplerDescriptorHeap[ samplerIndex ];

	return float4( skybox.Sample( skyboxSampler, input.direction ).rgb, 1.0 );
}
