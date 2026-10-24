#include "Ldx12_ConstantBuffers.hlsli"

struct CubeData
{
	float4x4 modelViewProjection;
	float4x4 model;
};

struct SphereData
{
	float4x4 modelViewProjection;
	float4 position;
};

struct LightData
{
	float4 position;
	float4 color;
};

struct VertexInput
{
	float3 position : POSITION;
	float3 normal : NORMAL;
};

struct VertexOutput
{
	float4 position : SV_Position;
	float3 worldPosition : TEXCOORD0;
	float3 normal : NORMAL;
};

VertexOutput VSCube( VertexInput input )
{
	CubeData cube = CB0.Load<CubeData>();

	VertexOutput output;
	output.position = mul( float4( input.position, 1.0 ), cube.modelViewProjection );
	output.worldPosition = mul( float4( input.position, 1.0 ), cube.model ).xyz;
	// Uniform scale keeps normal directions.
	output.normal = normalize( mul( input.normal, (float3x3)cube.model ) );
	return output;
}

float4 PSCube( VertexOutput input ) : SV_Target0
{
	LightData light = CB1.Load<LightData>();

	const float3 toLight = light.position.xyz - input.worldPosition;
	const float distanceSquared = max( dot( toLight, toLight ), 0.0001 );
	const float3 lightDirection = toLight * rsqrt( distanceSquared );
	const float diffuse = saturate( dot( normalize( input.normal ), lightDirection ) );
	const float attenuation = 4.0 / ( 1.0 + 0.25 * distanceSquared );
	const float3 redMaterial = float3( 1.0, 0.025, 0.015 );
	const float3 color = redMaterial * ( 0.025 + light.color.rgb * diffuse * attenuation );

	return float4( pow( saturate( color ), 1.0 / 2.2 ), 1.0 );
}

VertexOutput VSSphere( VertexInput input )
{
	SphereData sphere = CB0.Load<SphereData>();

	VertexOutput output;
	output.position = mul( float4( input.position, 1.0 ), sphere.modelViewProjection );
	output.worldPosition = input.position * sphere.position.w + sphere.position.xyz;
	output.normal = input.normal;
	return output;
}

float4 PSSphere() : SV_Target0
{
	LightData light = CB1.Load<LightData>();

	return float4( pow( saturate( light.color.rgb ), 1.0 / 2.2 ), light.color.a );
}
