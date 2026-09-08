#include "Ldx12/TestTemplate.hpp"
#include "Ldx12/Ldx12Native.hpp"
#include "Ldx12/HLSLLoader.hpp"
#include "../samples/GltfSampleCommon/SceneMaterials.hpp"
#include "../samples/GltfSampleCommon/SceneEnvironment.hpp"

#include <iostream>

using namespace ldx12;
using namespace ldx12::tests;

int main()
{
	try
	{
		DeviceManagerGuard guard;
		ContextDesc context;
		DeviceManager& manager = DeviceManager::Initialize( context );
		guard.active = true;

		RenderDevice& device = *manager.GetRenderDevice();
		D3D12Native native = device.GetNative();
		const std::filesystem::path media = LDX12_GLTF_MEDIA_DIRECTORY;
		const utils::GltfScene scene = utils::LoadGltfScene( media / "gltfMesh/DamagedHelmet.gltf" );

		gltf_sample::SceneMaterials resources = gltf_sample::UploadMaterials( device, scene );
		Require( resources.images.size() == 5, "The helmet must upload all five maps." );
		Require( resources.samplers.size() == 1, "The helmet's shared sampler must be deduplicated." );
		Require( resources.samplers[ 0 ].desc.filter == D3D12_FILTER_ANISOTROPIC && resources.samplers[ 0 ].desc.maxAnisotropy == 16,
			"Unspecified glTF filters should use anisotropic x16." );

		uint32_t colorMaps = 0;
		for( const gltf_sample::UploadedImage& image : resources.images )
		{
			const D3D12_RESOURCE_DESC desc = native.GetResource( image.texture )->GetDesc();
			Require( desc.Width == 2048 && desc.Height == 2048 && desc.MipLevels == 12, "Expected 2048x2048 with 12 mip levels." );

			if( image.format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB )
			{
				++colorMaps;
			}
		}
		Require( colorMaps == 2, "Only albedo and emission should use sRGB." );

		const gltf_sample::MaterialBindings& bindings = resources.bindings.at( 0 );
		Require( bindings.baseColor.texture && bindings.normal.texture && bindings.metallicRoughness.texture &&
			bindings.occlusion.texture && bindings.emissive.texture, "A material map has no GPU binding." );
		Require( bindings.baseColor.sampler == bindings.normal.sampler && bindings.baseColor.sampler == bindings.metallicRoughness.sampler &&
			bindings.baseColor.sampler == bindings.occlusion.sampler && bindings.baseColor.sampler == bindings.emissive.sampler,
			"The helmet's texture-to-sampler associations were lost." );

		const gltf_sample::SceneEnvironment environment = gltf_sample::LoadEnvironment( device, media / "pbr" );
		Require( environment.specular.size() == 9, "Expected nine prefiltered environment levels." );

		for( size_t level = 0; level < environment.specular.size(); ++level )
		{
			const D3D12_RESOURCE_DESC desc = native.GetResource( environment.specular[ level ] )->GetDesc();
			Require( desc.DepthOrArraySize == 6 && desc.Width == ( 256u >> level ), "Incorrect environment cube level." );
		}

		// Compile the actual PBR shader, including all bindless samplers and IBL inputs.
		HLSLLoader::SetRootDirectory( std::filesystem::path( LDX12_GLTF_SAMPLE_DIRECTORY ) );
		RenderPipelineDesc pipelineDesc;
		pipelineDesc.vertexShader = HLSLLoader::LoadStage( "shaders/GltfScene.hlsl", "vs_6_6", "VSMain" );
		pipelineDesc.fragmentShader = HLSLLoader::LoadStage( "shaders/GltfScene.hlsl", "ps_6_6", "PSMain" );
		pipelineDesc.colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		pipelineDesc.inputElements[ 0 ].semanticName = "POSITION";
		pipelineDesc.inputElements[ 0 ].format = DXGI_FORMAT_R32G32B32_FLOAT;
		pipelineDesc.inputElements[ 1 ].semanticName = "NORMAL";
		pipelineDesc.inputElements[ 1 ].format = DXGI_FORMAT_R32G32B32_FLOAT;
		pipelineDesc.inputElements[ 1 ].alignedByteOffset = 12;
		pipelineDesc.inputElements[ 2 ].semanticName = "TEXCOORD";
		pipelineDesc.inputElements[ 2 ].format = DXGI_FORMAT_R32G32_FLOAT;
		pipelineDesc.inputElements[ 2 ].alignedByteOffset = 24;
		RenderPipelineState pipeline = device.CreateRenderPipeline( pipelineDesc );
		Require( pipeline.Valid(), "PBR pipeline creation failed." );

		device.WaitIdle();
		gltf_sample::DestroyMaterials( device, resources );
		gltf_sample::DestroyEnvironment( device, environment );

		std::cout << "PBR verified: five 2048x2048 maps, 12 mips each, two sRGB maps, shared anisotropic x16 sampler, nine HDR environment levels and PBR shaders.\n";
		return 0;
	}
	catch( const std::exception& error )
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
