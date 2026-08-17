#include "App/OrbitCamera.hpp"

#include <algorithm>
#include <cmath>

namespace App
{
	using namespace DirectX;

	void OrbitCamera::Orbit( float deltaX, float deltaY )
	{
		yaw_ += deltaX * 0.007f;
		pitch_ = std::clamp( pitch_ + deltaY * 0.007f, -XM_PIDIV2 + 0.02f, XM_PIDIV2 - 0.02f );
	}

	void OrbitCamera::Pan( float deltaX, float deltaY )
	{
		const XMFLOAT3 position = Position();
		const XMVECTOR eye = XMLoadFloat3( &position );
		const XMVECTOR target = XMLoadFloat3( &target_ );
		const XMVECTOR forward = XMVector3Normalize( target - eye );
		const XMVECTOR right = XMVector3Normalize( XMVector3Cross( XMVectorSet( 0, 1, 0, 0 ), forward ) );
		const XMVECTOR up = XMVector3Normalize( XMVector3Cross( forward, right ) );
		const float scale = distance_ * 0.0015f;
		XMVECTOR moved = target + right * ( -deltaX * scale ) + up * ( deltaY * scale );
		XMStoreFloat3( &target_, moved );
	}

	void OrbitCamera::Zoom( float wheelDelta )
	{
		distance_ *= std::pow( 0.85f, wheelDelta );
		distance_ = std::clamp( distance_, std::max( 0.01f, focusRadius_ * 0.03f ), std::max( 100.0f, focusRadius_ * 100.0f ) );
	}

	void OrbitCamera::MoveLocal( float rightAmount, float upAmount, float forwardAmount, float deltaSeconds )
	{
		const XMFLOAT3 position = Position();
		const XMVECTOR eye = XMLoadFloat3( &position );
		XMVECTOR target = XMLoadFloat3( &target_ );
		const XMVECTOR forward = XMVector3Normalize( target - eye );
		const XMVECTOR right = XMVector3Normalize( XMVector3Cross( XMVectorSet( 0, 1, 0, 0 ), forward ) );
		const XMVECTOR up = XMVectorSet( 0, 1, 0, 0 );
		const float speed = std::max( 0.5f, distance_ ) * deltaSeconds;
		target += right * rightAmount * speed + up * upAmount * speed + forward * forwardAmount * speed;
		XMStoreFloat3( &target_, target );
	}

	void OrbitCamera::FocusBounds( const XMFLOAT3& minimum, const XMFLOAT3& maximum )
	{
		target_ = { ( minimum.x + maximum.x ) * 0.5f, ( minimum.y + maximum.y ) * 0.5f, ( minimum.z + maximum.z ) * 0.5f };
		const float extentX = maximum.x - minimum.x;
		const float extentY = maximum.y - minimum.y;
		const float extentZ = maximum.z - minimum.z;
		focusRadius_ = std::max( 0.001f, std::sqrt( extentX * extentX + extentY * extentY + extentZ * extentZ ) * 0.5f );
		distance_ = focusRadius_ * 2.6f;
	}

	XMFLOAT3 OrbitCamera::Position() const
	{
		const float horizontal = std::cos( pitch_ ) * distance_;
		return {
			target_.x + std::sin( yaw_ ) * horizontal,
			target_.y + std::sin( pitch_ ) * distance_,
			target_.z - std::cos( yaw_ ) * horizontal
		};
	}

	XMMATRIX OrbitCamera::ViewMatrix() const
	{
		const XMFLOAT3 position = Position();
		return XMMatrixLookAtLH( XMLoadFloat3( &position ), XMLoadFloat3( &target_ ), XMVectorSet( 0, 1, 0, 0 ) );
	}

	XMMATRIX OrbitCamera::ProjectionMatrix( float aspectRatio ) const
	{
		const float nearPlane = std::max( 0.001f, std::min( 0.1f, distance_ * 0.005f ) );
		const float farPlane = std::max( 1000.0f, distance_ + focusRadius_ * 20.0f );
		return XMMatrixPerspectiveFovLH( XMConvertToRadians( 55.0f ), std::max( 0.01f, aspectRatio ), nearPlane, farPlane );
	}

	const XMFLOAT3& OrbitCamera::Target() const noexcept { return target_; }
	float OrbitCamera::Distance() const noexcept { return distance_; }
}
