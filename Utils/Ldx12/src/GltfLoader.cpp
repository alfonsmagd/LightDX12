#include "Ldx12Utils/GltfLoader.hpp"

#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <cstring>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <string>

using namespace DirectX;

namespace ldx12::utils
{
	namespace
	{
		using GltfDocument = std::unique_ptr<cgltf_data, decltype( &cgltf_free )>;

		GltfDocument ReadDocument( const std::filesystem::path& path )
		{
			cgltf_options options{};
			cgltf_data* parsed = nullptr;
			const std::string filename = path.string();

			const cgltf_result result = cgltf_parse_file( &options, filename.c_str(), &parsed );
			GltfDocument document( parsed, cgltf_free );

			if( result != cgltf_result_success )
			{
				throw std::runtime_error( "Cannot open glTF: " + filename );
			}

			if( cgltf_load_buffers( &options, document.get(), filename.c_str() ) != cgltf_result_success ||
				cgltf_validate( document.get() ) != cgltf_result_success )
			{
				throw std::runtime_error( "Invalid glTF geometry or missing buffers: " + filename );
			}

			return document;
		}


		std::vector<ImageRgba8> LoadImages( const cgltf_data& document, const std::filesystem::path& directory )
		{
			std::vector<ImageRgba8> images;

			for( size_t index = 0; index < document.images_count; ++index )
			{
				const cgltf_image& source = document.images[ index ];

				if( source.buffer_view )
				{
					const uint8_t* bytes = cgltf_buffer_view_data( source.buffer_view );
					images.push_back( LoadImageRgba8( std::span<const uint8_t>( bytes, source.buffer_view->size ) ) );
				}
				else if( source.uri )
				{
					std::string filename = source.uri;
					filename.resize( cgltf_decode_uri( filename.data() ) );
					images.push_back( LoadImageRgba8( directory / filename ) );
				}
				else
				{
					throw std::runtime_error( "glTF image has no file or embedded data." );
				}
			}

			return images;
		}


		D3D12_TEXTURE_ADDRESS_MODE ReadWrapMode( cgltf_wrap_mode mode )
		{
			switch( mode )
			{
				case cgltf_wrap_mode_clamp_to_edge: return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
				case cgltf_wrap_mode_mirrored_repeat: return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
				default: return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			}
		}


		SamplerDesc ReadSampler( const cgltf_sampler* source )
		{
			// glTF leaves unspecified filtering to the viewer; use anisotropic x16.
			SamplerDesc sampler;
			sampler.filter = D3D12_FILTER_ANISOTROPIC;
			sampler.maxAnisotropy = 16;
			sampler.addressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
			sampler.addressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

			if( !source )
			{
				return sampler;
			}

			sampler.addressU = ReadWrapMode( source->wrap_s );
			sampler.addressV = ReadWrapMode( source->wrap_t );

			if( source->min_filter == cgltf_filter_type_undefined && source->mag_filter == cgltf_filter_type_undefined )
			{
				return sampler;
			}

			sampler.maxAnisotropy = 1;

			D3D12_FILTER_TYPE minification = D3D12_FILTER_TYPE_LINEAR;
			D3D12_FILTER_TYPE mip = D3D12_FILTER_TYPE_LINEAR;
			const D3D12_FILTER_TYPE magnification = source->mag_filter == cgltf_filter_type_nearest
				? D3D12_FILTER_TYPE_POINT : D3D12_FILTER_TYPE_LINEAR;

			switch( source->min_filter )
			{
				case cgltf_filter_type_nearest:
					minification = D3D12_FILTER_TYPE_POINT;
					sampler.maxLod = 0;
					break;

				case cgltf_filter_type_linear:
					sampler.maxLod = 0;
					break;

				case cgltf_filter_type_nearest_mipmap_nearest:
					minification = D3D12_FILTER_TYPE_POINT;
					mip = D3D12_FILTER_TYPE_POINT;
					break;

				case cgltf_filter_type_linear_mipmap_nearest:
					mip = D3D12_FILTER_TYPE_POINT;
					break;

				case cgltf_filter_type_nearest_mipmap_linear:
					minification = D3D12_FILTER_TYPE_POINT;
					break;

				default:
					break;
			}

			sampler.filter = D3D12_ENCODE_BASIC_FILTER( minification, magnification, mip, D3D12_FILTER_REDUCTION_TYPE_STANDARD );

			return sampler;
		}


		GltfTexture ReadTexture( const cgltf_texture_view& view, const cgltf_data& document )
		{
			GltfTexture texture;

			if( view.texture && view.texture->image )
			{
				texture.image = static_cast<int32_t>( view.texture->image - document.images );
				texture.sampler = ReadSampler( view.texture->sampler );
			}

			return texture;
		}


		void ReadMaterial( const cgltf_material* source, const cgltf_data& document, GltfPrimitive& primitive )
		{
			if( !source )
			{
				return;
			}

			const cgltf_pbr_metallic_roughness& pbr = source->pbr_metallic_roughness;

			std::memcpy( &primitive.baseColor, pbr.base_color_factor, sizeof( primitive.baseColor ) );
			std::memcpy( &primitive.emissive, source->emissive_factor, sizeof( primitive.emissive ) );

			primitive.metallic = pbr.metallic_factor;
			primitive.roughness = pbr.roughness_factor;
			primitive.normalScale = source->normal_texture.scale;
			primitive.occlusionStrength = source->occlusion_texture.scale;
			primitive.alphaCutoff = source->alpha_mode == cgltf_alpha_mode_mask ? source->alpha_cutoff : -1.0f;

			primitive.baseColorTexture = ReadTexture( pbr.base_color_texture, document );
			primitive.metallicRoughnessTexture = ReadTexture( pbr.metallic_roughness_texture, document );
			primitive.normalTexture = ReadTexture( source->normal_texture, document );
			primitive.occlusionTexture = ReadTexture( source->occlusion_texture, document );
			primitive.emissiveTexture = ReadTexture( source->emissive_texture, document );
		}


		std::vector<GeometryVertex> ReadVertices( const cgltf_primitive& primitive )
		{
			const cgltf_accessor* positions = cgltf_find_accessor( &primitive, cgltf_attribute_type_position, 0 );
			const cgltf_accessor* normals = cgltf_find_accessor( &primitive, cgltf_attribute_type_normal, 0 );
			const cgltf_accessor* texCoords = cgltf_find_accessor( &primitive, cgltf_attribute_type_texcoord, 0 );

			if( !positions || positions->count == 0 )
			{
				throw std::runtime_error( "glTF mesh has no positions." );
			}

			std::vector<GeometryVertex> vertices( positions->count );

			for( size_t index = 0; index < vertices.size(); ++index )
			{
				GeometryVertex& vertex = vertices[ index ];
				cgltf_accessor_read_float( positions, index, &vertex.position.x, 3 );

				if( normals )
				{
					cgltf_accessor_read_float( normals, index, &vertex.normal.x, 3 );
				}

				if( texCoords )
				{
					cgltf_accessor_read_float( texCoords, index, &vertex.texCoord.x, 2 );
				}
			}

			return vertices;
		}


		std::vector<uint32_t> ReadIndices( const cgltf_primitive& primitive, size_t vertexCount )
		{
			const size_t count = primitive.indices ? primitive.indices->count : vertexCount;
			std::vector<uint32_t> indices( count );

			if( primitive.indices )
			{
				cgltf_accessor_unpack_indices( primitive.indices, indices.data(), sizeof( uint32_t ), count );
			}
			else
			{
				std::iota( indices.begin(), indices.end(), 0u );
			}

			return indices;
		}


		void GenerateNormals( GltfPrimitive& primitive )
		{
			for( size_t index = 0; index + 2 < primitive.indices.size(); index += 3 )
			{
				GeometryVertex& a = primitive.vertices.at( primitive.indices[ index ] );
				GeometryVertex& b = primitive.vertices.at( primitive.indices[ index + 1 ] );
				GeometryVertex& c = primitive.vertices.at( primitive.indices[ index + 2 ] );

				const XMVECTOR edgeAB = XMLoadFloat3( &b.position ) - XMLoadFloat3( &a.position );
				const XMVECTOR edgeAC = XMLoadFloat3( &c.position ) - XMLoadFloat3( &a.position );
				const XMVECTOR faceNormal = XMVector3Cross( edgeAB, edgeAC );

				XMStoreFloat3( &a.normal, XMLoadFloat3( &a.normal ) + faceNormal );
				XMStoreFloat3( &b.normal, XMLoadFloat3( &b.normal ) + faceNormal );
				XMStoreFloat3( &c.normal, XMLoadFloat3( &c.normal ) + faceNormal );
			}
		}


		void TransformVertices( GltfPrimitive& primitive, const cgltf_node& node )
		{
			XMFLOAT4X4 nodeTransform;
			cgltf_node_transform_world( &node, &nodeTransform._11 );

			// Bake the node hierarchy and flip Z for the left-handed renderer.
			const XMMATRIX world = XMLoadFloat4x4( &nodeTransform ) * XMMatrixScaling( 1, 1, -1 );
			const XMMATRIX normalMatrix = XMMatrixTranspose( XMMatrixInverse( nullptr, world ) );

			for( GeometryVertex& vertex : primitive.vertices )
			{
				const XMVECTOR position = XMVector3TransformCoord( XMLoadFloat3( &vertex.position ), world );
				const XMVECTOR normal = XMVector3TransformNormal( XMLoadFloat3( &vertex.normal ), normalMatrix );

				XMStoreFloat3( &vertex.position, position );
				XMStoreFloat3( &vertex.normal, XMVector3Normalize( normal ) );
			}

			// A reflection also reverses the triangle winding.
			if( XMVectorGetX( XMMatrixDeterminant( world ) ) < 0 )
			{
				for( size_t index = 0; index + 2 < primitive.indices.size(); index += 3 )
				{
					std::swap( primitive.indices[ index + 1 ], primitive.indices[ index + 2 ] );
				}
			}
		}


		GltfPrimitive LoadPrimitive( const cgltf_primitive& source, const cgltf_node& node, const cgltf_data& document )
		{
			if( source.type != cgltf_primitive_type_triangles )
			{
				throw std::runtime_error( "This demo loads triangle meshes only." );
			}

			GltfPrimitive primitive;
			primitive.vertices = ReadVertices( source );
			primitive.indices = ReadIndices( source, primitive.vertices.size() );
			ReadMaterial( source.material, document, primitive );

			if( !cgltf_find_accessor( &source, cgltf_attribute_type_normal, 0 ) )
			{
				GenerateNormals( primitive );
			}

			TransformVertices( primitive, node );

			return primitive;
		}


		void LoadNode( const cgltf_node& node, const cgltf_data& document, GltfScene& scene )
		{
			if( node.mesh )
			{
				for( size_t index = 0; index < node.mesh->primitives_count; ++index )
				{
					scene.primitives.push_back( LoadPrimitive( node.mesh->primitives[ index ], node, document ) );
				}
			}

			for( size_t index = 0; index < node.children_count; ++index )
			{
				LoadNode( *node.children[ index ], document, scene );
			}
		}


		void CalculateBounds( GltfScene& scene )
		{
			const XMFLOAT3& firstPosition = scene.primitives.front().vertices.front().position;
			XMVECTOR minimum = XMLoadFloat3( &firstPosition );
			XMVECTOR maximum = minimum;

			for( const GltfPrimitive& primitive : scene.primitives )
			{
				for( const GeometryVertex& vertex : primitive.vertices )
				{
					const XMVECTOR position = XMLoadFloat3( &vertex.position );
					minimum = XMVectorMin( minimum, position );
					maximum = XMVectorMax( maximum, position );
				}
			}

			XMStoreFloat3( &scene.boundsMin, minimum );
			XMStoreFloat3( &scene.boundsMax, maximum );
		}
	}


	GltfScene LoadGltfScene( const std::filesystem::path& path )
	{
		// 1. Read the glTF/GLB file and its binary buffers.
		const GltfDocument document = ReadDocument( path );
		const cgltf_scene* source = document->scene;

		if( !source && document->scenes_count > 0 )
		{
			source = &document->scenes[ 0 ];
		}

		if( !source )
		{
			throw std::runtime_error( "glTF file has no scene." );
		}

		// 2. Decode textures into CPU RGBA pixels.
		GltfScene scene;
		scene.images = LoadImages( *document, path.parent_path() );

		// 3. Read each mesh and apply its node transform.
		for( size_t index = 0; index < source->nodes_count; ++index )
		{
			LoadNode( *source->nodes[ index ], *document, scene );
		}

		if( scene.primitives.empty() )
		{
			throw std::runtime_error( "glTF scene has no meshes." );
		}

		// 4. Calculate the bounds used to frame the scene.
		CalculateBounds( scene );

		return scene;
	}
}
