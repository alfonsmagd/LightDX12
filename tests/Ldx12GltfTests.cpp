#include "Ldx12Utils/GltfLoader.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>

using namespace ldx12::utils;

namespace
{
	void Require( bool condition, const char* message )
	{
		if( !condition ) throw std::runtime_error( message );
	}

	bool Near( float a, float b ) { return std::abs( a - b ) < 0.0001f; }

	void CheckScene( const GltfScene& scene )
	{
		Require( scene.primitives.size() == 3, "Expected three mesh instances in the default scene." );
		Require( Near( scene.boundsMin.x, -3 ) && Near( scene.boundsMin.y, 0 ) && Near( scene.boundsMin.z, -3 ) &&
			Near( scene.boundsMax.x, 3 ) && Near( scene.boundsMax.y, 3 ) && Near( scene.boundsMax.z, 3 ), "Incorrect world-space bounds." );
		bool cyanFound = false;
		bool mirroredFound = false;
		for( const GltfPrimitive& primitive : scene.primitives )
		{
			for( const GeometryVertex& vertex : primitive.vertices )
			{
				const float length = std::sqrt( vertex.normal.x * vertex.normal.x + vertex.normal.y * vertex.normal.y + vertex.normal.z * vertex.normal.z );
				Require( Near( length, 1 ), "Normals must be normalized." );
			}
			for( size_t index = 0; index < primitive.indices.size(); index += 3 )
			{
				using namespace DirectX;
				const GeometryVertex& a = primitive.vertices.at( primitive.indices[ index ] );
				const GeometryVertex& b = primitive.vertices.at( primitive.indices[ index + 1 ] );
				const GeometryVertex& c = primitive.vertices.at( primitive.indices[ index + 2 ] );
				const XMVECTOR face = XMVector3Cross( XMLoadFloat3( &b.position ) - XMLoadFloat3( &a.position ), XMLoadFloat3( &c.position ) - XMLoadFloat3( &a.position ) );
				Require( XMVectorGetX( XMVector3Dot( face, XMLoadFloat3( &a.normal ) ) ) > 0, "Triangle winding and normal disagree." );
			}
			if( Near( primitive.baseColor.x, 0.05f ) )
			{
				cyanFound = true;
				const DirectX::XMFLOAT3& position = primitive.vertices[ 0 ].position;
				Require( Near( position.x, -2.5f ) && Near( position.y, 1 ) && Near( position.z, 1 ), "Parent translation or handedness conversion failed." );
				Require( primitive.indices.size() == 18, "Non-indexed triangles were not converted." );
			}
			if( Near( primitive.baseColor.x, 0.9f ) )
			{
				mirroredFound = true;
				const DirectX::XMFLOAT3& position = primitive.vertices[ 0 ].position;
				Require( Near( position.x, 0.75f ) && Near( position.y, 1 ) && Near( position.z, -0.75f ), "Mirrored node transform failed." );
			}
		}
		Require( cyanFound && mirroredFound, "Base colors were not preserved." );
	}

	void ExpectFailure( const std::filesystem::path& path )
	{
		try { LoadGltfScene( path ); }
		catch( const std::runtime_error& ) { return; }
		throw std::runtime_error( "Invalid or unsupported scene was accepted." );
	}
}

int main()
{
	const std::filesystem::path root = LDX12_GLTF_FIXTURE_DIRECTORY;
	const std::filesystem::path temporary = std::filesystem::temp_directory_path() / ( "Ldx12Gltf-" + std::to_string( GetCurrentProcessId() ) );
	try
	{
		for( const char* name : { "Scene.gltf", "Scene.glb", "SceneEmbedded.gltf" } ) CheckScene( LoadGltfScene( root / name ) );
		std::filesystem::create_directory( temporary );
		std::ifstream input( root / "SceneEmbedded.gltf" );
		const std::string original( ( std::istreambuf_iterator<char>( input ) ), std::istreambuf_iterator<char>() );
		const std::filesystem::path invalid = temporary / "Invalid.gltf";
		const auto reject = [&]( const std::string& text )
		{
			{ std::ofstream output( invalid ); output << text; }
			ExpectFailure( invalid );
		};
		reject( "{ malformed JSON" );
		std::string unsupported = original;
		const size_t mode = unsupported.find( "\"mode\": 4" );
		Require( mode != std::string::npos, "Fixture has no triangle mode." );
		unsupported.replace( mode, 9, "\"mode\": 1" );
		reject( unsupported );
		std::string invalidCount = original;
		const size_t count = invalidCount.find( "\"count\": 18" );
		Require( count != std::string::npos, "Fixture has no pyramid accessor." );
		invalidCount.replace( count, 11, "\"count\": 999999" );
		reject( invalidCount );
		ExpectFailure( temporary / "Missing.gltf" );
		std::filesystem::copy_file( root / "Scene.gltf", temporary / "MissingBuffer.gltf" );
		ExpectFailure( temporary / "MissingBuffer.gltf" );
		std::filesystem::remove( invalid );
		std::filesystem::remove( temporary / "MissingBuffer.gltf" );
		std::filesystem::remove( temporary );
		std::cout << "glTF tests passed: external buffers, base64, GLB, hierarchy, mirrored transforms, normals, materials and invalid inputs.\n";
		return 0;
	}
	catch( const std::exception& error )
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
