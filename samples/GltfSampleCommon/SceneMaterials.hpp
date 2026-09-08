#pragma once

#include "Ldx12Utils/GltfLoader.hpp"

namespace gltf_sample
{
	struct TextureBinding
	{
		uint32_t texture = 0;
		uint32_t sampler = 0;
	};


	struct MaterialBindings
	{
		TextureBinding baseColor;
		TextureBinding metallicRoughness;
		TextureBinding normal;
		TextureBinding occlusion;
		TextureBinding emissive;
	};


	struct UploadedImage
	{
		int32_t image = -1;
		DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
		ldx12::TextureHandle texture;
	};


	struct UploadedSampler
	{
		ldx12::SamplerDesc desc;
		ldx12::SamplerHandle handle;
	};


	struct SceneMaterials
	{
		std::vector<UploadedImage> images;
		std::vector<UploadedSampler> samplers;
		std::vector<MaterialBindings> bindings;
	};


	inline ldx12::TextureHandle UploadImage( ldx12::RenderDevice& device, const ldx12::utils::GltfScene& scene,
		int32_t imageIndex, DXGI_FORMAT format, SceneMaterials& resources )
	{
		// An image can be reused, including as both a color map and a data map.
		for( const UploadedImage& uploaded : resources.images )
		{
			if( uploaded.image == imageIndex && uploaded.format == format )
			{
				return uploaded.texture;
			}
		}

		const ldx12::utils::ImageRgba8& image = scene.images.at( imageIndex );

		ldx12::TextureDesc desc;
		desc.width = image.width;
		desc.height = image.height;
		desc.format = format;
		desc.countMipMap = 16; // Ldx12 clamps this to the image's full mip chain.
		desc.data = image.pixels.data();
		desc.rowPitch = image.width * 4;
		desc.slicePitch = desc.rowPitch * image.height;

		const ldx12::TextureHandle texture = device.CreateTexture( desc );
		resources.images.push_back( { imageIndex, format, texture } );

		return texture;
	}


	inline uint32_t UploadSampler( ldx12::RenderDevice& device, const ldx12::SamplerDesc& desc, SceneMaterials& resources )
	{
		// These are the fields populated by the glTF loader. Share identical samplers.
		for( const UploadedSampler& uploaded : resources.samplers )
		{
			const ldx12::SamplerDesc& existing = uploaded.desc;

			if( existing.filter == desc.filter && existing.addressU == desc.addressU &&
				existing.addressV == desc.addressV && existing.maxLod == desc.maxLod && existing.maxAnisotropy == desc.maxAnisotropy )
			{
				return device.GetSamplerIndex( uploaded.handle );
			}
		}

		const ldx12::SamplerHandle handle = device.CreateSampler( desc );
		resources.samplers.push_back( { desc, handle } );

		return device.GetSamplerIndex( handle );
	}


	inline TextureBinding UploadTexture( ldx12::RenderDevice& device, const ldx12::utils::GltfScene& scene,
		const ldx12::utils::GltfTexture& source, bool srgb, SceneMaterials& resources )
	{
		if( source.image < 0 )
		{
			return {};
		}

		const DXGI_FORMAT format = srgb ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;
		const ldx12::TextureHandle texture = UploadImage( device, scene, source.image, format, resources );

		TextureBinding binding;
		binding.texture = device.GetBindlessIndex( texture );
		binding.sampler = UploadSampler( device, source.sampler, resources );

		return binding;
	}


	inline void DestroyMaterials( ldx12::RenderDevice& device, SceneMaterials& resources );


	inline SceneMaterials UploadMaterials( ldx12::RenderDevice& device, const ldx12::utils::GltfScene& scene )
	{
		SceneMaterials resources;
		resources.images.reserve( scene.images.size() * 2 );
		resources.samplers.reserve( ldx12::ourCustomSamplerCount );
		resources.bindings.reserve( scene.primitives.size() );

		try
		{
			for( const ldx12::utils::GltfPrimitive& primitive : scene.primitives )
			{
				MaterialBindings material;

				material.baseColor = UploadTexture( device, scene, primitive.baseColorTexture, true, resources );
				material.metallicRoughness = UploadTexture( device, scene, primitive.metallicRoughnessTexture, false, resources );
				material.normal = UploadTexture( device, scene, primitive.normalTexture, false, resources );
				material.occlusion = UploadTexture( device, scene, primitive.occlusionTexture, false, resources );
				material.emissive = UploadTexture( device, scene, primitive.emissiveTexture, true, resources );

				resources.bindings.push_back( material );
			}
		}
		catch( ... )
		{
			device.WaitIdle();
			DestroyMaterials( device, resources );
			throw;
		}

		return resources;
	}


	inline void DestroyMaterials( ldx12::RenderDevice& device, SceneMaterials& resources )
	{
		for( const UploadedImage& image : resources.images )
		{
			device.Destroy( image.texture );
		}

		for( const UploadedSampler& sampler : resources.samplers )
		{
			device.Destroy( sampler.handle );
		}

		resources = {};
	}
}
