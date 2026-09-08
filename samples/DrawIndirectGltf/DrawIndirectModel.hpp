#pragma once

#include "../GltfScene/SceneMaterials.hpp"

#include <DirectXMath.h>

#include <cstdint>
#include <filesystem>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace draw_indirect_gltf_sample
{
	struct MaterialData
	{
		DirectX::XMFLOAT4 baseColor = { 1, 1, 1, 1 };
		DirectX::XMFLOAT3 emissive = {};
		float metallic = 1;

		float roughness = 1;
		float normalScale = 1;
		float occlusionStrength = 1;
		float alphaCutoff = -1;

		gltf_sample::TextureBinding baseColorTexture;
		gltf_sample::TextureBinding metallicRoughnessTexture;
		gltf_sample::TextureBinding normalTexture;
		gltf_sample::TextureBinding occlusionTexture;
		gltf_sample::TextureBinding emissiveTexture;
	};

	static_assert( sizeof( MaterialData ) == 88 );

	struct DrawData
	{
		uint32_t materialId = 0;
	};

	struct IndirectDraw
	{
		uint32_t drawId = 0;
		D3D12_DRAW_INDEXED_ARGUMENTS arguments = {};
	};

	static_assert( sizeof( DrawData ) == 4 );
	static_assert( sizeof( IndirectDraw ) == 24 );

	struct DrawIndirectModel
	{
		ldx12::utils::GltfScene scene;
		gltf_sample::SceneMaterials materialResources;
		ldx12::BufferHandle vertexBuffer;
		ldx12::BufferHandle indexBuffer;
		ldx12::BufferHandle indirectBuffer;
		ldx12::BufferHandle drawDataBuffer;
		ldx12::BufferHandle materialBuffer;
		std::filesystem::path path;
		uint32_t drawCount = 0;
		uint32_t vertexCount = 0;
		uint32_t indexCount = 0;
	};

	inline MaterialData BuildMaterial( const ldx12::utils::GltfPrimitive& primitive,
		const gltf_sample::MaterialBindings& bindings )
	{
		MaterialData material;
		material.baseColor = primitive.baseColor;
		material.emissive = primitive.emissive;
		material.metallic = primitive.metallic;
		material.roughness = primitive.roughness;
		material.normalScale = primitive.normalScale;
		material.occlusionStrength = primitive.occlusionStrength;
		material.alphaCutoff = primitive.alphaCutoff;
		material.baseColorTexture = bindings.baseColor;
		material.metallicRoughnessTexture = bindings.metallicRoughness;
		material.normalTexture = bindings.normal;
		material.occlusionTexture = bindings.occlusion;
		material.emissiveTexture = bindings.emissive;
		return material;
	}

	inline void DestroyModel( ldx12::RenderDevice& device, DrawIndirectModel& model )
	{
		if( model.materialBuffer.Valid() ) device.Destroy( model.materialBuffer );
		if( model.drawDataBuffer.Valid() ) device.Destroy( model.drawDataBuffer );
		if( model.indirectBuffer.Valid() ) device.Destroy( model.indirectBuffer );
		if( model.indexBuffer.Valid() ) device.Destroy( model.indexBuffer );
		if( model.vertexBuffer.Valid() ) device.Destroy( model.vertexBuffer );
		gltf_sample::DestroyMaterials( device, model.materialResources );
		model = {};
	}

	inline ldx12::BufferHandle CreateBuffer( ldx12::RenderDevice& device,
		const char* debugName,
		ldx12::BufferType type,
		uint32_t stride,
		const void* data,
		uint64_t size )
	{
		ldx12::BufferDesc desc;
		desc.debugName = debugName;
		desc.type = type;
		desc.stride = stride;
		desc.size = size;
		desc.initialData = data;
		return device.CreateBuffer( desc );
	}

	inline DrawIndirectModel LoadModel( ldx12::RenderDevice& device, const std::filesystem::path& path )
	{
		DrawIndirectModel model;
		model.path = path;
		model.scene = ldx12::utils::LoadGltfScene( path );

		try
		{
			model.materialResources = gltf_sample::UploadMaterials( device, model.scene );

			uint64_t totalVertices = 0;
			uint64_t totalIndices = 0;
			for( const ldx12::utils::GltfPrimitive& primitive : model.scene.primitives )
			{
				totalVertices += primitive.vertices.size();
				totalIndices += primitive.indices.size();
			}

			if( model.scene.primitives.size() > UINT32_MAX || totalVertices > static_cast<uint64_t>( std::numeric_limits<int32_t>::max() ) ||
				totalIndices > UINT32_MAX )
			{
				throw std::length_error( "glTF scene is too large for one indexed indirect batch." );
			}

			std::vector<ldx12::utils::GeometryVertex> vertices;
			std::vector<uint32_t> indices;
			std::vector<MaterialData> materials;
			std::vector<DrawData> drawData;
			std::vector<IndirectDraw> indirectDraws;
			vertices.reserve( static_cast<size_t>( totalVertices ) );
			indices.reserve( static_cast<size_t>( totalIndices ) );
			materials.reserve( model.scene.primitives.size() );
			drawData.reserve( model.scene.primitives.size() );
			indirectDraws.reserve( model.scene.primitives.size() );

			for( uint32_t drawId = 0; drawId < model.scene.primitives.size(); ++drawId )
			{
				const ldx12::utils::GltfPrimitive& primitive = model.scene.primitives[ drawId ];
				const uint32_t firstIndex = static_cast<uint32_t>( indices.size() );
				const int32_t baseVertex = static_cast<int32_t>( vertices.size() );

				vertices.insert( vertices.end(), primitive.vertices.begin(), primitive.vertices.end() );
				indices.insert( indices.end(), primitive.indices.begin(), primitive.indices.end() );
				materials.push_back( BuildMaterial( primitive, model.materialResources.bindings[ drawId ] ) );
				drawData.push_back( { drawId } );

				IndirectDraw command;
				command.drawId = drawId;
				command.arguments.IndexCountPerInstance = static_cast<uint32_t>( primitive.indices.size() );
				command.arguments.InstanceCount = 1;
				command.arguments.StartIndexLocation = firstIndex;
				command.arguments.BaseVertexLocation = baseVertex;
				command.arguments.StartInstanceLocation = 0;
				indirectDraws.push_back( command );
			}

			model.vertexBuffer = CreateBuffer( device, "DrawIndirectGltf vertices", ldx12::BufferType::Vertex,
				sizeof( ldx12::utils::GeometryVertex ), vertices.data(), vertices.size() * sizeof( vertices.front() ) );
			model.indexBuffer = CreateBuffer( device, "DrawIndirectGltf indices", ldx12::BufferType::Index,
				sizeof( uint32_t ), indices.data(), indices.size() * sizeof( indices.front() ) );
			model.indirectBuffer = CreateBuffer( device, "DrawIndirectGltf commands", ldx12::BufferType::Indirect,
				sizeof( IndirectDraw ), indirectDraws.data(), indirectDraws.size() * sizeof( indirectDraws.front() ) );
			model.drawDataBuffer = CreateBuffer( device, "DrawIndirectGltf draw data", ldx12::BufferType::Structured,
				sizeof( DrawData ), drawData.data(), drawData.size() * sizeof( drawData.front() ) );
			model.materialBuffer = CreateBuffer( device, "DrawIndirectGltf materials", ldx12::BufferType::Structured,
				sizeof( MaterialData ), materials.data(), materials.size() * sizeof( materials.front() ) );

			model.drawCount = static_cast<uint32_t>( indirectDraws.size() );
			model.vertexCount = static_cast<uint32_t>( vertices.size() );
			model.indexCount = static_cast<uint32_t>( indices.size() );

			for( ldx12::utils::GltfPrimitive& primitive : model.scene.primitives )
			{
				primitive.vertices = {};
				primitive.indices = {};
			}
			model.scene.images = {};
		}
		catch( ... )
		{
			device.WaitIdle();
			DestroyModel( device, model );
			throw;
		}

		return model;
	}

	inline void ReplaceModel( ldx12::RenderDevice& device, DrawIndirectModel& model, const std::filesystem::path& path )
	{
		DrawIndirectModel loaded = LoadModel( device, path );
		device.WaitIdle();
		DestroyModel( device, model );
		model = std::move( loaded );
	}
}
