#include "App/ObjMesh.hpp"

#include "App/TaskSystem.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace App
{
	namespace
	{
		using DirectX::XMFLOAT3;

		struct ObjIndex
		{
			int position = 0;
			int normal = 0;

			bool operator==( const ObjIndex& other ) const noexcept
			{
				return position == other.position && normal == other.normal;
			}
		};

		struct ObjIndexHash
		{
			size_t operator()( const ObjIndex& value ) const noexcept
			{
				return static_cast<size_t>( static_cast<uint32_t>( value.position ) ) ^
					( static_cast<size_t>( static_cast<uint32_t>( value.normal ) ) << 32u );
			}
		};

		XMFLOAT3 Subtract( const XMFLOAT3& a, const XMFLOAT3& b )
		{
			return { a.x - b.x, a.y - b.y, a.z - b.z };
		}

		XMFLOAT3 Cross( const XMFLOAT3& a, const XMFLOAT3& b )
		{
			return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
		}

		void Add( XMFLOAT3& destination, const XMFLOAT3& value )
		{
			destination.x += value.x;
			destination.y += value.y;
			destination.z += value.z;
		}

		void Normalize( XMFLOAT3& value )
		{
			const float length = std::sqrt( value.x * value.x + value.y * value.y + value.z * value.z );
			if( length > 1.0e-8f )
			{
				value.x /= length;
				value.y /= length;
				value.z /= length;
			}
			else value = { 0.0f, 1.0f, 0.0f };
		}

		int ResolveIndex( int index, size_t count )
		{
			if( index > 0 ) return index - 1;
			if( index < 0 ) return static_cast<int>( count ) + index;
			return -1;
		}

		ObjIndex ParseFaceIndex( const std::string& token, size_t positionCount, size_t normalCount )
		{
			ObjIndex result;
			const size_t firstSlash = token.find( '/' );
			const std::string positionText = token.substr( 0, firstSlash );
			if( positionText.empty() ) throw std::runtime_error( "OBJ face without a position index." );
			result.position = ResolveIndex( std::stoi( positionText ), positionCount );
			if( firstSlash != std::string::npos )
			{
				const size_t secondSlash = token.find( '/', firstSlash + 1 );
				if( secondSlash != std::string::npos && secondSlash + 1 < token.size() )
					result.normal = ResolveIndex( std::stoi( token.substr( secondSlash + 1 ) ), normalCount ) + 1;
			}
			if( result.position < 0 || static_cast<size_t>( result.position ) >= positionCount )
				throw std::runtime_error( "OBJ face position index is out of range." );
			if( result.normal < 0 || ( result.normal > 0 && static_cast<size_t>( result.normal - 1 ) >= normalCount ) )
				throw std::runtime_error( "OBJ face normal index is out of range." );
			return result;
		}
	}

	uint64_t ObjMeshData::CpuBytes() const noexcept
	{
		return vertices.capacity() * sizeof( MeshVertex ) + indices.capacity() * sizeof( uint32_t );
	}
	uint64_t ObjMeshData::GpuBytes() const noexcept
	{
		return vertices.size() * sizeof( MeshVertex ) + indices.size() * sizeof( uint32_t );
	}
	uint32_t ObjMeshData::TriangleCount() const noexcept { return static_cast<uint32_t>( indices.size() / 3 ); }

	ObjMeshData LoadObj( const std::filesystem::path& path )
	{
		std::ifstream file( path );
		if( !file ) throw std::runtime_error( "No se puede abrir el fichero OBJ." );

		ObjMeshData mesh;
		mesh.sourcePath = path;
		std::error_code sizeError;
		mesh.fileBytes = std::filesystem::file_size( path, sizeError );
		std::vector<XMFLOAT3> positions;
		std::vector<XMFLOAT3> normals;
		std::vector<bool> needsGeneratedNormal;
		std::unordered_map<ObjIndex, uint32_t, ObjIndexHash> vertexMap;

		std::string line;
		uint64_t lineNumber = 0;
		while( std::getline( file, line ) )
		{
			++lineNumber;
			std::istringstream stream( line );
			std::string command;
			stream >> command;
			if( command == "v" )
			{
				XMFLOAT3 position;
				if( !( stream >> position.x >> position.y >> position.z ) )
					throw std::runtime_error( "Invalid OBJ vertex at line " + std::to_string( lineNumber ) + "." );
				positions.push_back( position );
			}
			else if( command == "vn" )
			{
				XMFLOAT3 normal;
				if( !( stream >> normal.x >> normal.y >> normal.z ) )
					throw std::runtime_error( "Invalid OBJ normal at line " + std::to_string( lineNumber ) + "." );
				Normalize( normal );
				normals.push_back( normal );
			}
			else if( command == "f" )
			{
				std::vector<uint32_t> polygon;
				std::string token;
				while( stream >> token )
				{
					if( !token.empty() && token.front() == '#' ) break;
					const ObjIndex key = ParseFaceIndex( token, positions.size(), normals.size() );
					auto found = vertexMap.find( key );
					if( found == vertexMap.end() )
					{
						MeshVertex vertex;
						vertex.position = positions[static_cast<size_t>( key.position )];
						const bool generated = key.normal == 0;
						if( !generated ) vertex.normal = normals[static_cast<size_t>( key.normal - 1 )];
						const uint32_t index = static_cast<uint32_t>( mesh.vertices.size() );
						mesh.vertices.push_back( vertex );
						needsGeneratedNormal.push_back( generated );
						vertexMap.emplace( key, index );
						polygon.push_back( index );
					}
					else polygon.push_back( found->second );
				}
				if( polygon.size() < 3 )
					throw std::runtime_error( "OBJ face has fewer than three vertices at line " + std::to_string( lineNumber ) + "." );
				for( size_t index = 1; index + 1 < polygon.size(); ++index )
				{
					const uint32_t a = polygon[0];
					const uint32_t b = polygon[index];
					const uint32_t c = polygon[index + 1];
					mesh.indices.insert( mesh.indices.end(), { a, b, c } );
					const XMFLOAT3 faceNormal = Cross( Subtract( mesh.vertices[b].position, mesh.vertices[a].position ), Subtract( mesh.vertices[c].position, mesh.vertices[a].position ) );
					if( needsGeneratedNormal[a] ) Add( mesh.vertices[a].normal, faceNormal );
					if( needsGeneratedNormal[b] ) Add( mesh.vertices[b].normal, faceNormal );
					if( needsGeneratedNormal[c] ) Add( mesh.vertices[c].normal, faceNormal );
				}
			}
		}

		if( mesh.vertices.empty() || mesh.indices.empty() ) throw std::runtime_error( "OBJ contains no renderable triangles." );
		for( size_t index = 0; index < mesh.vertices.size(); ++index )
			if( needsGeneratedNormal[index] ) Normalize( mesh.vertices[index].normal );

		const float maximum = std::numeric_limits<float>::max();
		mesh.boundsMin = { maximum, maximum, maximum };
		mesh.boundsMax = { -maximum, -maximum, -maximum };
		for( const MeshVertex& vertex : mesh.vertices )
		{
			mesh.boundsMin.x = std::min( mesh.boundsMin.x, vertex.position.x );
			mesh.boundsMin.y = std::min( mesh.boundsMin.y, vertex.position.y );
			mesh.boundsMin.z = std::min( mesh.boundsMin.z, vertex.position.z );
			mesh.boundsMax.x = std::max( mesh.boundsMax.x, vertex.position.x );
			mesh.boundsMax.y = std::max( mesh.boundsMax.y, vertex.position.y );
			mesh.boundsMax.z = std::max( mesh.boundsMax.z, vertex.position.z );
		}
		return mesh;
	}

	ObjAssetService::ObjAssetService( TaskSystem& tasks ): tasks_( &tasks ) {}

	bool ObjAssetService::ScanAsync( std::vector<std::filesystem::path> roots )
	{
		bool expected = false;
		if( !scanning_.compare_exchange_strong( expected, true ) ) return false;
		tasks_->Submit( "Escanear catalogo OBJ", [this, roots = std::move( roots )]()
		{
			ObjCatalogEvent event;
			try
			{
				for( const auto& root : roots )
				{
					std::error_code error;
					if( !std::filesystem::exists( root, error ) ) continue;
					for( std::filesystem::recursive_directory_iterator iterator( root, std::filesystem::directory_options::skip_permission_denied, error ), end; iterator != end; iterator.increment( error ) )
					{
						if( error ) { error.clear(); continue; }
						std::string extension = iterator->path().extension().string();
						std::transform( extension.begin(), extension.end(), extension.begin(), []( unsigned char value ) { return static_cast<char>( std::tolower( value ) ); } );
						if( iterator->is_regular_file( error ) && extension == ".obj" ) event.files.push_back( iterator->path() );
					}
				}
				std::sort( event.files.begin(), event.files.end() );
				event.files.erase( std::unique( event.files.begin(), event.files.end() ), event.files.end() );
			}
			catch( const std::exception& exception ) { event.error = exception.what(); }
			catalogEvents_.Push( std::move( event ) );
			scanning_.store( false );
		} );
		return true;
	}

	bool ObjAssetService::LoadAsync( std::filesystem::path path )
	{
		bool expected = false;
		if( !loading_.compare_exchange_strong( expected, true ) ) return false;
		tasks_->Submit( "Parsear OBJ: " + path.filename().string(), [this, path = std::move( path )]()
		{
			ObjLoadEvent event;
			event.path = path;
			try { event.mesh = LoadObj( path ); }
			catch( const std::exception& exception ) { event.error = exception.what(); }
			loadEvents_.Push( std::move( event ) );
			loading_.store( false );
		} );
		return true;
	}

	bool ObjAssetService::IsScanning() const noexcept { return scanning_.load(); }
	bool ObjAssetService::IsLoading() const noexcept { return loading_.load(); }
	std::optional<ObjCatalogEvent> ObjAssetService::TryPopCatalogEvent() { return catalogEvents_.TryPop(); }
	std::optional<ObjLoadEvent> ObjAssetService::TryPopLoadEvent() { return loadEvents_.TryPop(); }
}
