#include "Ldx12/Ldx12.hpp"
#include "Ldx12Utils/Ldx12Utils.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>

using namespace ldx12;
using namespace ldx12::utils;

namespace
{
	void Require( bool condition, const char* message )
	{
		if( !condition )
		{
			throw std::runtime_error( message );
		}
	}

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
		const MeshHandle cube = world.AddCube( CubeDesc{} );
		SphereDesc sphereDesc{};
		sphereDesc.transform.position = { 1.5f, 0.0f, 0.0f };
		const MeshHandle sphere = world.AddSphere( sphereDesc );
		Require( world.NumMeshes() == 2, "World did not retain its two mesh descriptions." );

		RenderWorldDesc renderWorldDesc{};
		renderWorldDesc.colorFormat = colorDesc.format;
		renderWorldDesc.depthFormat = depthDesc.format;
		{
			RenderWorld renderWorld( device, renderWorldDesc );
			Require( renderWorld.GetVertexCount() == 0 && renderWorld.GetIndexCount() == 0,
				"World geometry was generated before the first Render call." );

			Camera camera{};
			camera.aspectRatio = 1.0f;
			RenderFrame( device, renderWorld, world, camera, color, depth );
			Require( renderWorld.GetVertexCount() == 449, "Combined cube and sphere vertex count is incorrect." );
			Require( renderWorld.GetIndexCount() == 2340, "Combined cube and sphere index count is incorrect." );
			Require( renderWorld.GetDrawCount() == 2, "The indirect buffer does not contain one draw per object." );

			World secondWorld;
			secondWorld.AddSphere( SphereDesc{} );
			secondWorld.AddSphere( SphereDesc{} );
			RenderFrame( device, renderWorld, secondWorld, camera, color, depth );
			Require( renderWorld.GetVertexCount() == 850 && renderWorld.GetIndexCount() == 4608,
				"RenderWorld did not rebuild for a different World with the same revision." );
			RenderFrame( device, renderWorld, world, camera, color, depth );

			Transform movedCube{};
			movedCube.position = { -2.0f, 0.5f, 0.0f };
			Require( world.SetTransform( cube, movedCube ), "Failed to modify a live world transform." );
			RenderFrame( device, renderWorld, world, camera, color, depth );
			Require( renderWorld.GetVertexCount() == 449 && renderWorld.GetDrawCount() == 2,
				"Changing a transform unexpectedly changed the geometry batch." );

			Require( world.Destroy( cube ), "Failed to destroy a live cube." );
			Require( !world.Destroy( cube ), "A stale mesh handle destroyed an object twice." );
			RenderFrame( device, renderWorld, world, camera, color, depth );
			Require( renderWorld.GetVertexCount() == 425, "The cube was not removed from the lazy geometry rebuild." );
			Require( renderWorld.GetIndexCount() == 2304 && renderWorld.GetDrawCount() == 1,
				"The sphere batch was not preserved after deleting the cube." );

			Require( world.Contains( sphere ), "The sphere handle became invalid after deleting another object." );
			world.Clear();
			RenderFrame( device, renderWorld, world, camera, color, depth );
			Require( renderWorld.GetVertexCount() == 0 && renderWorld.GetIndexCount() == 0 && renderWorld.GetDrawCount() == 0,
				"Clearing the world did not clear the rendered batch." );
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
