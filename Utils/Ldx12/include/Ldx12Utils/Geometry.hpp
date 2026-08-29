#pragma once

#include "Ldx12/Ldx12.hpp"

#include <DirectXMath.h>

#include <cstdint>

namespace ldx12::utils
{
	struct GeometryVertex
	{
		DirectX::XMFLOAT3 position = {};
		DirectX::XMFLOAT3 normal = {};
		DirectX::XMFLOAT2 texCoord = {};
	};

	struct GeometryBuffers
	{
		BufferHandle vertexBuffer = {};
		BufferHandle indexBuffer = {};
		uint32_t indexCount = 0;
	};

	GeometryBuffers CreateCube( RenderDevice& device );
	GeometryBuffers CreateSphere( RenderDevice& device, uint32_t rings = 16, uint32_t segments = 32 );
	void DestroyGeometry( RenderDevice& device, GeometryBuffers& geometry );
}
