#pragma once

#include "Ldx12Internal.hpp"

namespace ldx12
{
	class DeviceManager::Impl;

	SubmitHandle SubmitCommandBufferBatch( DeviceManager::Impl& impl, ICommandBuffer* const* commandBuffers, uint32_t commandBufferCount, TextureHandle presentTexture );
}
