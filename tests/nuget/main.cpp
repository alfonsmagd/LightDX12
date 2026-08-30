#include <Ldx12/Ldx12.hpp>
#include <Ldx12Utils/OrbitCamera.hpp>

int main()
{
	ldx12::utils::OrbitCamera camera{};
	const DirectX::XMVECTOR cameraPosition = camera.GetPosition();
	ldx12::DeviceManager::ShutdownSingleton();
	return DirectX::XMVectorGetW( cameraPosition ) == 1.0f ? 0 : 1;
}
