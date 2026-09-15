#pragma once

#include "Ldx12/Ldx12.hpp"

namespace gltf_sample
{
	struct SceneTargets
	{
		ldx12::TextureHandle color;
		ldx12::TextureHandle depth;
		uint32_t width = 0;
		uint32_t height = 0;
	};


	inline void DestroyTargets( ldx12::RenderDevice& device, SceneTargets& targets )
	{
		if( targets.color.Valid() )
		{
			device.Destroy( targets.color );
			device.Destroy( targets.depth );
		}

		targets = {};
	}


	inline void ResizeTargets( ldx12::RenderDevice& device, SceneTargets& targets,
		uint32_t width, uint32_t height, DXGI_FORMAT colorFormat )
	{
		if( targets.width == width && targets.height == height )
		{
			return;
		}

		DestroyTargets( device, targets );

		ldx12::TextureDesc desc;
		desc.width = width;
		desc.height = height;
		desc.sampleCount = 4;
		desc.format = colorFormat;
		desc.usage = ldx12::TextureUsage::RenderTarget;
		targets.color = device.CreateTexture( desc );

		desc.format = DXGI_FORMAT_D32_FLOAT;
		desc.usage = ldx12::TextureUsage::DepthStencil;
		targets.depth = device.CreateTexture( desc );

		targets.width = width;
		targets.height = height;
	}
}
