#pragma once

#include "SceneMaterials.hpp"

namespace gltf_sample
{
	struct SceneModel
	{
		ldx12::utils::GltfScene scene;
		SceneMaterials materials;
		std::vector<ldx12::utils::GeometryBuffers> geometry;
		std::filesystem::path path;
	};


	inline void DestroyModel( ldx12::RenderDevice& device, SceneModel& model )
	{
		for( ldx12::utils::GeometryBuffers& buffers : model.geometry )
		{
			if( buffers.indexBuffer.Valid() ) device.Destroy( buffers.indexBuffer );
			if( buffers.vertexBuffer.Valid() ) device.Destroy( buffers.vertexBuffer );
		}

		DestroyMaterials( device, model.materials );
		model = {};
	}


	inline SceneModel LoadModel( ldx12::RenderDevice& device, const std::filesystem::path& path )
	{
		SceneModel model;
		model.path = path;
		model.scene = ldx12::utils::LoadGltfScene( path );

		try
		{
			model.materials = UploadMaterials( device, model.scene );
			model.geometry.resize( model.scene.primitives.size() );

			for( size_t index = 0; index < model.geometry.size(); ++index )
			{
				ldx12::utils::GltfPrimitive& primitive = model.scene.primitives[ index ];
				ldx12::utils::GeometryBuffers& buffers = model.geometry[ index ];

				ldx12::BufferDesc desc;
				desc.type = ldx12::BufferType::Vertex;
				desc.size = primitive.vertices.size() * sizeof( ldx12::utils::GeometryVertex );
				desc.stride = sizeof( ldx12::utils::GeometryVertex );
				desc.initialData = primitive.vertices.data();
				buffers.vertexBuffer = device.CreateBuffer( desc );

				desc.type = ldx12::BufferType::Index;
				desc.size = primitive.indices.size() * sizeof( uint32_t );
				desc.stride = sizeof( uint32_t );
				desc.initialData = primitive.indices.data();
				buffers.indexBuffer = device.CreateBuffer( desc );
				buffers.indexCount = static_cast<uint32_t>( primitive.indices.size() );

				// Keep material properties and bounds; geometry now lives on the GPU.
				primitive.vertices = {};
				primitive.vertices.shrink_to_fit();
				primitive.indices = {};
				primitive.indices.shrink_to_fit();
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


	inline void ReplaceModel( ldx12::RenderDevice& device, SceneModel& model, const std::filesystem::path& path )
	{
		SceneModel loaded = LoadModel( device, path );
		device.WaitIdle();
		DestroyModel( device, model );
		model = std::move( loaded );
	}
}
