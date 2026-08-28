#include "Ldx12/Ldx12.hpp"
#include "Ldx12Utils/Ldx12Utils.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <stdexcept>

using namespace ldx12;
using namespace ldx12::utils;

namespace
{
	void RenderFrame( RenderDevice& device, RenderWorld& renderWorld, const World& world, const Camera& camera, TextureHandle color, TextureHandle depth )
	{
		ICommandBuffer& commands = device.AcquireCommandBuffer();
		RenderPass renderPass{};
		renderPass.color[ 0 ].loadOp = LoadOp::Clear;
		renderPass.depthStencil.depthLoadOp = LoadOp::Clear;
		Framebuffer framebuffer{};
		framebuffer.color[ 0 ].texture = color;
		framebuffer.depthStencil.texture = depth;
		commands.CmdBeginRendering( renderPass, framebuffer );
		renderWorld.Render( commands, world, camera );
		commands.CmdEndRendering();
		const SubmitHandle submission = device.Submit( commands );
		device.Wait( submission );
	}
}

int main()
{
	try
	{
		ContextDesc contextDesc{};
		contextDesc.enableDebugLayer = true;
		DeviceManager& manager = DeviceManager::Initialize( contextDesc );
		RenderDevice& device = *manager.GetRenderDevice();

		TextureDesc colorDesc{};
		colorDesc.debugName = "Ldx12 Utils Test Color";
		colorDesc.width = 128;
		colorDesc.height = 128;
		colorDesc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
		colorDesc.usage = TextureUsage::RenderTarget;
		const TextureHandle color = device.CreateTexture( colorDesc );

		TextureDesc depthDesc{};
		depthDesc.debugName = "Ldx12 Utils Test Depth";
		depthDesc.width = 128;
		depthDesc.height = 128;
		depthDesc.format = DXGI_FORMAT_D32_FLOAT;
		depthDesc.usage = TextureUsage::DepthStencil;
		const TextureHandle depth = device.CreateTexture( depthDesc );

		World world;
		CubeDesc cubeDesc{};
		const ObjectHandle cube = world.AddCube( cubeDesc );
		cubeDesc.transform.position = { -1.5f, 0.0f, 0.0f };
		cubeDesc.size = { 2.0f, 0.5f, 1.0f };
		cubeDesc.color = { 1.0f, 0.0f, 0.0f, 1.0f };
		world.AddCube( cubeDesc );
		cubeDesc.transform.position = { 1.5f, 0.0f, 0.0f };
		cubeDesc.color = { 0.0f, 1.0f, 0.0f, 1.0f };
		world.AddCube( cubeDesc );

		SphereDesc sphereDesc{};
		sphereDesc.transform.position = { -1.0f, 1.5f, 0.0f };
		const ObjectHandle sphere = world.AddSphere( sphereDesc );
		sphereDesc.transform.position = { 1.0f, 1.5f, 0.0f };
		sphereDesc.color = { 1.0f, 0.0f, 1.0f, 1.0f };
		world.AddSphere( sphereDesc );

		ArrowDesc arrowDesc{};
		arrowDesc.start = { -2.0f, -1.0f, 0.0f };
		arrowDesc.end = { 2.0f, -1.0f, 0.0f };
		arrowDesc.color = { 1.0f, 0.0f, 0.0f, 1.0f };
		const ObjectHandle arrow = world.AddArrow( arrowDesc );
		arrowDesc.start = { 0.0f, -1.0f, -2.0f };
		arrowDesc.end = { 0.0f, 2.0f, 0.0f };
		arrowDesc.color = { 0.0f, 1.0f, 0.0f, 1.0f };
		world.AddArrow( arrowDesc );
		assert( world.NumObjects() == 7 );

		RenderWorldDesc renderWorldDesc{};
		renderWorldDesc.colorFormat = colorDesc.format;
		renderWorldDesc.depthFormat = depthDesc.format;
		{
			RenderWorld renderWorld( device, renderWorldDesc );
			assert( renderWorld.GetVertexCount() == 475 && renderWorld.GetIndexCount() == 2436 );

			Camera camera{};
			camera.aspectRatio = 1.0f;
			RenderFrame( device, renderWorld, world, camera, color, depth );
			assert( renderWorld.GetVertexCount() == 475 );
			assert( renderWorld.GetIndexCount() == 2436 );
			assert( renderWorld.GetDrawCount() == 3 );
			assert( renderWorld.GetInstanceCount() == 7 );

			World secondWorld;
			secondWorld.AddSphere( SphereDesc{} );
			secondWorld.AddSphere( SphereDesc{} );
			RenderFrame( device, renderWorld, secondWorld, camera, color, depth );
			assert( renderWorld.GetVertexCount() == 475 && renderWorld.GetIndexCount() == 2436 );
			assert( renderWorld.GetDrawCount() == 1 && renderWorld.GetInstanceCount() == 2 );
			RenderFrame( device, renderWorld, world, camera, color, depth );

			Transform movedCube{};
			movedCube.position = { -2.0f, 0.5f, 0.0f };
			world.SetTransform( cube, movedCube );
			RenderFrame( device, renderWorld, world, camera, color, depth );
			assert( renderWorld.GetVertexCount() == 475 );
			assert( renderWorld.GetDrawCount() == 3 );
			assert( renderWorld.GetInstanceCount() == 7 );

			world.Destroy( cube );
			RenderFrame( device, renderWorld, world, camera, color, depth );
			assert( renderWorld.GetVertexCount() == 475 );
			assert( renderWorld.GetIndexCount() == 2436 );
			assert( renderWorld.GetDrawCount() == 3 );
			assert( renderWorld.GetInstanceCount() == 6 );

			assert( world.Contains( sphere ) );
			assert( world.Contains( arrow ) );
			world.Clear();
			RenderFrame( device, renderWorld, world, camera, color, depth );
			assert( renderWorld.GetVertexCount() == 475 );
			assert( renderWorld.GetIndexCount() == 2436 );
			assert( renderWorld.GetDrawCount() == 0 );
			assert( renderWorld.GetInstanceCount() == 0 );
		}

		device.WaitIdle();
		device.Destroy( depth );
		device.Destroy( color );
		DeviceManager::ShutdownSingleton();
		std::cout << "Ldx12 Utils tests passed.\n";
		return 0;
	}
	catch( const std::exception& error )
	{
		DeviceManager::ShutdownSingleton();
		std::cerr << "Ldx12 Utils tests failed: " << error.what() << '\n';
		return 1;
	}
}
