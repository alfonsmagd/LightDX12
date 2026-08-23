#pragma once

#include "Ldx12Internal.hpp"

namespace ldx12
{
	SubmitHandle SubmitCommandBufferBatch( DeviceManager& manager, ICommandBuffer* const* commandBuffers, uint32_t commandBufferCount, TextureHandle presentTexture );
}
