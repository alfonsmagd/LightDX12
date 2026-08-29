#include "Ldx12Utils/OrbitCamera.hpp"

#include "Ldx12Utils/AppLdx.hpp"

namespace ldx12::utils
{
	void OrbitCamera::Update( const AppLdx& app )
	{
		if( !app.IsLeftMouseButtonDown() )
		{
			return;
		}

		yaw += static_cast<float>( app.GetMouseDeltaX() ) * sensitivity;
		pitch += static_cast<float>( app.GetMouseDeltaY() ) * sensitivity;
		if( pitch < -1.4f ) pitch = -1.4f;
		if( pitch > 1.4f ) pitch = 1.4f;
	}

	DirectX::XMVECTOR OrbitCamera::GetPosition() const noexcept
	{
		const float horizontalDistance = distance * DirectX::XMScalarCos( pitch );
		return DirectX::XMVectorSet(
			horizontalDistance * DirectX::XMScalarSin( yaw ),
			distance * DirectX::XMScalarSin( pitch ),
			horizontalDistance * DirectX::XMScalarCos( yaw ),
			1.0f );
	}

	DirectX::XMMATRIX OrbitCamera::GetViewMatrix() const noexcept
	{
		const DirectX::XMVECTOR position = GetPosition();
		const DirectX::XMVECTOR direction = DirectX::XMVector3Normalize( DirectX::XMVectorNegate( position ) );
		return DirectX::XMMatrixLookToLH(
			position,
			direction,
			DirectX::XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f ) );
	}

	DirectX::XMMATRIX OrbitCamera::GetSkyViewMatrix() const noexcept
	{
		const DirectX::XMVECTOR direction = DirectX::XMVector3Normalize( DirectX::XMVectorNegate( GetPosition() ) );
		return DirectX::XMMatrixLookToLH(
			DirectX::XMVectorZero(),
			direction,
			DirectX::XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f ) );
	}
}
