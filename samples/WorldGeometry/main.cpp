#include "Ldx12/Ldx12.hpp"
#include "Ldx12Utils/AppLdx.hpp"
#include "Ldx12Utils/DepthTarget.hpp"
#include "Ldx12Utils/Ldx12Utils.hpp"

#include <cstdint>
#include <stdexcept>

using namespace ldx12;
using namespace ldx12::utils;

int WINAPI wWinMain( HINSTANCE instance, HINSTANCE, PWSTR, int showCommand )
{
	try
	{
		constexpr uint32_t initialWidth = 1280;
		constexpr uint32_t initialHeight = 720;
		AppLdxDesc appDesc{};
		appDesc.instance = instance;
		appDesc.showCommand = showCommand;
		appDesc.className = L"Ldx12WorldGeometryWindow";
		appDesc.title = L"Ldx12 World - Instanced geometry and debug wireframes";
		appDesc.width = initialWidth;
		appDesc.height = initialHeight;
		AppLdx app( appDesc );

		ContextDesc contextDesc{};
		contextDesc.enableDebugLayer = true;
		SwapchainDesc swapchainDesc{};
		swapchainDesc.window = MakeWin32WindowHandle( app.GetWindow() );
		swapchainDesc.width = initialWidth;
		swapchainDesc.height = initialHeight;
		swapchainDesc.vsync = true;

		DeviceManager& manager = DeviceManager::Initialize( contextDesc, swapchainDesc );
		app.SetDeviceManager( manager );
		RenderDevice& device = *manager.GetRenderDevice();

		World world;
		CubeDesc cubeDesc{};
		cubeDesc.transform.position = { -3.0f, -0.6f, 0.0f };
		cubeDesc.transform.rotation = { 0.15f, 0.25f, 0.0f };
		cubeDesc.color = { 1.0f, 0.20f, 0.15f, 1.0f };
		world.AddCube( cubeDesc );

		cubeDesc.transform.position = { -1.5f, -0.2f, 0.4f };
		cubeDesc.transform.rotation = { 0.25f, 0.55f, 0.0f };
		cubeDesc.size = { 0.8f, 1.6f, 0.8f };
		cubeDesc.color = { 1.0f, 0.60f, 0.10f, 1.0f };
		world.AddCube( cubeDesc );

		cubeDesc.transform.position = { 0.0f, 0.1f, 0.0f };
		cubeDesc.transform.rotation = { 0.35f, 0.80f, 0.1f };
		cubeDesc.size = { 1.2f, 1.2f, 1.2f };
		cubeDesc.color = { 0.20f, 0.90f, 0.35f, 1.0f };
		world.AddCube( cubeDesc );

		cubeDesc.transform.position = { 1.5f, -0.2f, 0.4f };
		cubeDesc.transform.rotation = { 0.20f, 1.10f, 0.0f };
		cubeDesc.size = { 0.8f, 1.6f, 0.8f };
		cubeDesc.color = { 0.15f, 0.55f, 1.0f, 1.0f };
		world.AddCube( cubeDesc );

		cubeDesc.transform.position = { 3.0f, -0.6f, 0.0f };
		cubeDesc.transform.rotation = { 0.15f, 1.35f, 0.0f };
		cubeDesc.size = { 1.0f, 1.0f, 1.0f };
		cubeDesc.color = { 0.75f, 0.25f, 1.0f, 1.0f };
		world.AddCube( cubeDesc );

		SphereDesc sphereDesc{};
		sphereDesc.transform.position = { -2.25f, 1.15f, 0.2f };
		sphereDesc.radius = 0.8f;
		sphereDesc.color = { 0.20f, 0.95f, 1.0f, 1.0f };
		world.AddSphere( sphereDesc );

		sphereDesc.transform.position = { 2.25f, 1.15f, 0.2f };
		sphereDesc.color = { 1.0f, 0.25f, 0.75f, 1.0f };
		world.AddSphere( sphereDesc );

		SphereDesc debugSphereDesc{};
		debugSphereDesc.transform.position = { 0.0f, 1.75f, 0.3f };
		debugSphereDesc.radius = 1.0f;
		debugSphereDesc.color = { 1.0f, 0.95f, 0.20f, 1.0f };
		ObjectHandle debugSphere = world.AddSphere( debugSphereDesc );

		ArrowDesc arrowDesc{};
		arrowDesc.start = { -3.5f, -1.8f, 0.0f };
		arrowDesc.end = { -3.5f, 1.8f, 0.0f };
		arrowDesc.color = { 1.0f, 0.85f, 0.10f, 1.0f };
		world.AddArrow( arrowDesc );

		DebugRendererDesc debugRendererDesc{};
		debugRendererDesc.colorFormat = contextDesc.swapchainFormat;
		debugRendererDesc.depthFormat = DXGI_FORMAT_D32_FLOAT;
		{
			DebugRenderer debugRenderer( device, debugRendererDesc );
			DepthTarget depthTarget( device );
			Camera camera{};
			camera.position = { 0.0f, 3.0f, -10.0f };
			camera.target = { 0.0f, 0.4f, 0.0f };

			while( app.PumpMessages() )
			{
				if( app.IsWindowMinimized() )
				{
					WaitMessage();
					continue;
				}

				if( app.WasKeyPressed( VK_SPACE ) )
				{
					if( world.Contains( debugSphere ) )
					{
						world.Destroy( debugSphere );
					}
					else
					{
						debugSphere = world.AddSphere( debugSphereDesc );
					}
				}

				depthTarget.Resize( manager.GetWidth(), manager.GetHeight() );
				camera.aspectRatio = static_cast<float>( manager.GetWidth() ) / static_cast<float>( manager.GetHeight() );

				ICommandBuffer& commands = device.AcquireCommandBuffer();
				const TextureHandle backbuffer = device.GetCurrentSwapchainTexture();
				RenderPass renderPass{};
				renderPass.color[ 0 ].loadOp = LoadOp::Clear;
				renderPass.color[ 0 ].clearColor = { 0.025f, 0.035f, 0.055f, 1.0f };
				renderPass.depthStencil.depthLoadOp = LoadOp::Clear;
				renderPass.depthStencil.clearDepth = 1.0f;

				Framebuffer framebuffer{};
				framebuffer.color[ 0 ].texture = backbuffer;
				framebuffer.depthStencil.texture = depthTarget.GetTexture();

				commands.CmdBeginRendering( renderPass, framebuffer );
				debugRenderer.Render( commands, world, camera );
				commands.CmdEndRendering();
				device.Submit( commands, backbuffer );
			}

			device.WaitIdle();
		}

		DeviceManager::ShutdownSingleton();
		return 0;
	}
	catch( const std::exception& error )
	{
		DeviceManager::ShutdownSingleton();
		MessageBoxA( nullptr, error.what(), "Ldx12 World Geometry failed", MB_ICONERROR | MB_OK );
		return 1;
	}
}
