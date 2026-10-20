#include "Ldx12/Ldx12.hpp"
#include "Ldx12Utils/Geometry.hpp"
#include "Ldx12Utils/Ldx12Utils.hpp"
#include "Ldx12/TestTemplate.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>

using namespace ldx12;
using namespace ldx12::utils;
using ldx12::tests::Require;

namespace
{
	void RenderFrame( RenderDevice& device, DebugRenderer& debugRenderer, const World& world, const Camera& camera, TextureHandle color, TextureHandle depth )
	{
		CommandBuffer& commands = device.AcquireCommandBuffer();
		RenderPass renderPass{};
		renderPass.color[ 0 ].loadOp = LoadOp::Clear;
		renderPass.depthStencil.depthLoadOp = LoadOp::Clear;
		Framebuffer framebuffer{};
		framebuffer.color[ 0 ].texture = color;
		framebuffer.depthStencil.texture = depth;
		commands.CmdBeginRendering( renderPass, framebuffer );
		debugRenderer.Render( commands, world, camera );
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

		GeometryBuffers quadGeometry = CreateQuad( device );
		GeometryBuffers cubeGeometry = CreateCube( device );
		GeometryBuffers sphereGeometry = CreateSphere( device );

		Require( quadGeometry.vertexBuffer.Valid() && quadGeometry.indexBuffer.Valid() && quadGeometry.indexCount == 6,
			"CreateQuad did not produce buffers for two triangles." );
		Require( cubeGeometry.vertexBuffer.Valid() && cubeGeometry.indexBuffer.Valid() && cubeGeometry.indexCount == 36,
			"CreateCube did not produce buffers for twelve triangles." );
		Require( sphereGeometry.vertexBuffer.Valid() && sphereGeometry.indexBuffer.Valid() && sphereGeometry.indexCount > 0,
			"CreateSphere did not produce indexed geometry." );

		DestroyGeometry( device, sphereGeometry );
		DestroyGeometry( device, cubeGeometry );
		DestroyGeometry( device, quadGeometry );

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

		Require( world.NumObjects() == 7, "World did not retain three cubes, two spheres and two arrows." );

		DebugRendererDesc debugRendererDesc{};
		debugRendererDesc.colorFormat = colorDesc.format;
		debugRendererDesc.depthFormat = depthDesc.format;
		{
			DebugRenderer debugRenderer( device, debugRendererDesc );

			Require( debugRenderer.GetVertexCount() == 453 && debugRenderer.GetIndexCount() == 2346,
				"DebugRenderer did not build its shared cube, sphere and arrow meshes." );

			Camera camera{};
			camera.aspectRatio = 1.0f;
			RenderFrame( device, debugRenderer, world, camera, color, depth );

			Require( debugRenderer.GetDrawCount() == 3 && debugRenderer.GetInstanceCount() == 7,
				"Seven objects of three geometry types were not batched into three draws." );

			World secondWorld;
			secondWorld.AddSphere( SphereDesc{} );
			secondWorld.AddSphere( SphereDesc{} );
			RenderFrame( device, debugRenderer, secondWorld, camera, color, depth );

			Require( debugRenderer.GetVertexCount() == 453 && debugRenderer.GetIndexCount() == 2346,
				"Switching worlds rebuilt or duplicated the shared geometry." );
			Require( debugRenderer.GetDrawCount() == 1 && debugRenderer.GetInstanceCount() == 2,
				"Switching worlds retained draws or instances from the previous world." );

			RenderFrame( device, debugRenderer, world, camera, color, depth );

			Require( debugRenderer.GetDrawCount() == 3 && debugRenderer.GetInstanceCount() == 7,
				"Switching back did not restore the original world's instances." );

			Transform movedCube{};
			movedCube.position = { -2.0f, 0.5f, 0.0f };
			world.SetTransform( cube, movedCube );
			RenderFrame( device, debugRenderer, world, camera, color, depth );

			Require( debugRenderer.GetDrawCount() == 3 && debugRenderer.GetInstanceCount() == 7,
				"Changing an object's transform changed the draw or instance count." );

			world.Destroy( cube );
			RenderFrame( device, debugRenderer, world, camera, color, depth );

			Require( debugRenderer.GetDrawCount() == 3 && debugRenderer.GetInstanceCount() == 6, "Destroying one cube did not remove exactly one instance." );

			Require( world.Contains( sphere ) && world.Contains( arrow ), "Destroying a cube invalidated unrelated objects." );

			world.Clear();
			RenderFrame( device, debugRenderer, world, camera, color, depth );

			Require( debugRenderer.GetVertexCount() == 453 && debugRenderer.GetIndexCount() == 2346,
				"Clearing a world changed the renderer's shared geometry." );
			Require( debugRenderer.GetDrawCount() == 0 && debugRenderer.GetInstanceCount() == 0,
				"Rendering a cleared world retained stale draws or instances." );
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
