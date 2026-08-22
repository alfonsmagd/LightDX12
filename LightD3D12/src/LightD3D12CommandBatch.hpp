#pragma once

#include "LightD3D12Internal.hpp"

namespace lightd3d12
{
	class DeviceManager::Impl;

	SubmitHandle SubmitCommandBufferBatch( DeviceManager::Impl& impl, ICommandBuffer* const* commandBuffers, uint32_t commandBufferCount, TextureHandle presentTexture );
}
