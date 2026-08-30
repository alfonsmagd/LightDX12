#pragma once

#include "Ldx12Internal.hpp"

namespace ldx12
{
	class BaseMips final
	{
	public:
		explicit BaseMips( DeviceManager& manager );
		BaseMips( const BaseMips& ) = delete;
		BaseMips& operator=( const BaseMips& ) = delete;

		void Generate( TextureResource& texture, D3D12_RESOURCE_STATES finalState );

	private:
		struct PushConstants final
		{
			uint32_t sourceTextureIndex = 0;
			uint32_t destinationTextureIndex = 0;
			uint32_t sourceMipLevel = 0;
			uint32_t destinationWidth = 1;
			uint32_t destinationHeight = 1;
			uint32_t writeSrgb = 0;
		};

		static void TransitionSubresource( ID3D12GraphicsCommandList* commandList,
			ID3D12Resource* resource,
			uint32_t subresource,
			D3D12_RESOURCE_STATES before,
			D3D12_RESOURCE_STATES after ) noexcept;

		DeviceManager& manager_;
		ComPtr<ID3D12PipelineState> pipelineState_;
	};
}
