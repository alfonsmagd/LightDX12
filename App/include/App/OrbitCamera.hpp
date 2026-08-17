#pragma once

#include <DirectXMath.h>

namespace App
{
	class OrbitCamera final
	{
	public:
		void Orbit( float deltaX, float deltaY );
		void Pan( float deltaX, float deltaY );
		void Zoom( float wheelDelta );
		void MoveLocal( float right, float up, float forward, float deltaSeconds );
		void FocusBounds( const DirectX::XMFLOAT3& minimum, const DirectX::XMFLOAT3& maximum );

		DirectX::XMMATRIX ViewMatrix() const;
		DirectX::XMMATRIX ProjectionMatrix( float aspectRatio ) const;
		DirectX::XMFLOAT3 Position() const;
		const DirectX::XMFLOAT3& Target() const noexcept;
		float Distance() const noexcept;

	private:
		DirectX::XMFLOAT3 target_{ 0.0f, 0.0f, 0.0f };
		float yaw_ = 0.75f;
		float pitch_ = 0.35f;
		float distance_ = 5.0f;
		float focusRadius_ = 1.0f;
	};
}
