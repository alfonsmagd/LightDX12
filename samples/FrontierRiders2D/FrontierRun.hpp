#pragma once

#include "FrontierPhysics.hpp"

#include "LightD3D12/LightD3D12.hpp"

namespace frontier
{
	class FrontierRun final
	{
	public:
		void Initialize( lightd3d12::RenderDevice& device, DXGI_FORMAT colorFormat );
		void Shutdown();
		void Reset();

		void Physics( float deltaSeconds, const InputState& input );
		void Render( lightd3d12::ICommandBuffer& commands );

		const GameState& World() const noexcept;

	private:
		FrontierPhysics physics_;
	};
}
