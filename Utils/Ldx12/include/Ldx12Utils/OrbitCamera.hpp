#pragma once

#include <DirectXMath.h>

namespace ldx12::utils
{
	class AppLdx;

	struct OrbitCamera
	{
		float yaw = 0.7f;
		float pitch = 0.44f;
		float distance = 6.5f;
		float sensitivity = 0.005f;

		void Update( const AppLdx& app );
		DirectX::XMVECTOR GetPosition() const noexcept;
		DirectX::XMMATRIX GetViewMatrix() const noexcept;
		DirectX::XMMATRIX GetSkyViewMatrix() const noexcept;
	};
}
