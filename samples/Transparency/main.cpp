#include "Ldx12/HLSLLoader.hpp"
#include "Ldx12/Ldx12.hpp"
#include "Ldx12Utils/AppLdx.hpp"
#include "Ldx12Utils/DepthTarget.hpp"
#include "Ldx12Utils/Geometry.hpp"
#include "Ldx12Utils/OrbitCamera.hpp"
#include "Ldx12Utils/TextureLoader.hpp"

#include <DirectXMath.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <exception>
#include <filesystem>

using namespace DirectX;
using namespace ldx12;

namespace
{
	struct PushConstants
	{
		XMFLOAT4X4 viewProjection = {};
		XMFLOAT4X4 skyViewProjection = {};
		XMFLOAT4X4 model = {};
		XMFLOAT4 color = {};
		uint32_t cubeMapIndex = 0;
		uint32_t samplerIndex = 0;
		std::array<uint32_t, 2> padding = {};
	};

	struct TransparentQuad
	{
		XMFLOAT3 position = {};
		XMFLOAT4 color = {};
	};

	static_assert( sizeof( PushConstants ) == 224 );

	RenderPipelineState CreateOpaquePipeline( RenderDevice& device, DXGI_FORMAT colorFormat )
	{
		RenderPipelineDesc desc{};
		desc.vertexShader = HLSLLoader::LoadStage( "shaders/TransparencyQuad.hlsl", "vs_6_6", "VSMain" );
		desc.fragmentShader = HLSLLoader::LoadStage( "shaders/TransparencyQuad.hlsl", "ps_6_6", "PSMain" );
		desc.color[ 0 ].format = colorFormat;
		desc.depthFormat = DXGI_FORMAT_D32_FLOAT;
		desc.inputElements[ 0 ].semanticName = "POSITION";
		desc.inputElements[ 0 ].format = DXGI_FORMAT_R32G32B32_FLOAT;
		desc.rasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		desc.depthStencilState.DepthEnable = TRUE;
		desc.depthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		desc.depthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		desc.depthStencilState.StencilEnable = FALSE;
		desc.blendState.RenderTarget[ 0 ].BlendEnable = FALSE;
		return device.CreateRenderPipeline( desc );
	}

	RenderPipelineState CreateTransparentPipeline( RenderDevice& device, DXGI_FORMAT colorFormat )
	{
		RenderPipelineDesc desc{};
		desc.vertexShader = HLSLLoader::LoadStage( "shaders/TransparencyQuad.hlsl", "vs_6_6", "VSMain" );
		desc.fragmentShader = HLSLLoader::LoadStage( "shaders/TransparencyQuad.hlsl", "ps_6_6", "PSMain" );
		desc.color[ 0 ].format = colorFormat;
		desc.depthFormat = DXGI_FORMAT_D32_FLOAT;
		desc.inputElements[ 0 ].semanticName = "POSITION";
		desc.inputElements[ 0 ].format = DXGI_FORMAT_R32G32B32_FLOAT;
		desc.rasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		desc.depthStencilState.DepthEnable = TRUE;
		desc.depthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		desc.depthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		desc.depthStencilState.StencilEnable = FALSE;

		D3D12_RENDER_TARGET_BLEND_DESC& blend = desc.blendState.RenderTarget[ 0 ];
		blend.BlendEnable = TRUE;
		blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		blend.BlendOp = D3D12_BLEND_OP_ADD;
		blend.SrcBlendAlpha = D3D12_BLEND_ONE;
		blend.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
		blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
		return device.CreateRenderPipeline( desc );
	}

	float DistanceSquared( const XMFLOAT3& left, const XMFLOAT3& right )
	{
		const float x = left.x - right.x;
		const float y = left.y - right.y;
		const float z = left.z - right.z;
		return x * x + y * y + z * z;
	}

	RenderPipelineState CreateSkyboxPipeline( RenderDevice& device, DXGI_FORMAT colorFormat )
	{
		RenderPipelineDesc desc{};
		desc.vertexShader = HLSLLoader::LoadStage( "shaders/TransparencySkybox.hlsl", "vs_6_6", "VSMain" );
		desc.fragmentShader = HLSLLoader::LoadStage( "shaders/TransparencySkybox.hlsl", "ps_6_6", "PSMain" );
		desc.color[ 0 ].format = colorFormat;
		desc.depthFormat = DXGI_FORMAT_D32_FLOAT;
		desc.rasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		desc.depthStencilState.DepthEnable = TRUE;
		desc.depthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		desc.depthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		desc.depthStencilState.StencilEnable = FALSE;
		return device.CreateRenderPipeline( desc );
	}

}

int WINAPI wWinMain( HINSTANCE instance, HINSTANCE, PWSTR, int showCommand )
{
	try
	{
		constexpr uint32_t initialWidth = 1280;
		constexpr uint32_t initialHeight = 720;

		utils::AppLdxDesc appDesc{};
		appDesc.instance = instance;
		appDesc.showCommand = showCommand;
		appDesc.className = L"Ldx12TransparencyWindow";
		appDesc.title = L"Ldx12 - Transparency - Alpha 0.25 / 0.50 / 0.75";
		appDesc.width = initialWidth;
		appDesc.height = initialHeight;
		utils::AppLdx app( appDesc );

		HLSLLoader::SetRootDirectory( std::filesystem::path( __FILE__ ).parent_path() );

		ContextDesc context{};
		context.enableDebugLayer = true;

		SwapchainDesc swapchain{};
		swapchain.window = MakeWin32WindowHandle( app.GetWindow() );
		swapchain.width = initialWidth;
		swapchain.height = initialHeight;
		swapchain.vsync = true;

		DeviceManager& manager = DeviceManager::Initialize( context, swapchain );
		app.SetDeviceManager( manager );
		RenderDevice& device = *manager.GetRenderDevice();

		RenderPipelineState skyboxPipeline = CreateSkyboxPipeline( device, context.swapchainFormat );
		RenderPipelineState opaquePipeline = CreateOpaquePipeline( device, context.swapchainFormat );
		RenderPipelineState transparentPipeline = CreateTransparentPipeline( device, context.swapchainFormat );

		const std::filesystem::path cubeMapDirectory =
			std::filesystem::path( LDX12_MEDIA_DIRECTORY ) / "sky_129_cubemap_2k";

		const TextureHandle cubeMap = utils::LoadCubeMap( device, cubeMapDirectory );
		utils::GeometryBuffers cubeGeometry = utils::CreateCube( device );
		utils::GeometryBuffers quadGeometry = utils::CreateQuad( device );

		const std::array<TransparentQuad, 3> quads =
		{
			TransparentQuad{ { -2.2f, 0.0f,  1.0f }, { 1.0f, 0.0f, 0.0f, 0.25f } },
			TransparentQuad{ {  0.0f, 0.0f,  0.0f }, { 0.0f, 1.0f, 0.0f, 0.50f } },
			TransparentQuad{ {  2.2f, 0.0f, -1.0f }, { 0.0f, 0.3f, 1.0f, 0.95f } }
		};

		{
			utils::OrbitCamera camera;
			utils::DepthTarget depthTarget( device );

			while( app.PumpMessages() )
			{
				if( app.IsWindowMinimized() )
				{
					WaitMessage();
					continue;
				}

				camera.Update( app );
				const uint32_t width = manager.GetWidth();
				const uint32_t height = manager.GetHeight();
				depthTarget.Resize( width, height );
				const float aspect = static_cast<float>( width ) / static_cast<float>( height );
				const XMMATRIX projection = XMMatrixPerspectiveFovLH(
					XMConvertToRadians( 60.0f ), aspect, 0.1f, 100.0f );

				PushConstants constants{};
		XMStoreFloat4x4(
					&constants.viewProjection,
					XMMatrixTranspose( camera.GetViewMatrix() * projection ) );
				XMStoreFloat4x4(
					&constants.skyViewProjection,
					XMMatrixTranspose( camera.GetSkyViewMatrix() * projection ) );
				constants.cubeMapIndex = device.GetBindlessIndex( cubeMap );
				constants.samplerIndex = ToSamplerIndex( SamplerSlot::LinearClamp );

				std::array<TransparentQuad, 3> sortedQuads = quads;
				XMFLOAT3 cameraPosition{};
				XMStoreFloat3( &cameraPosition, camera.GetPosition() );
				std::sort(
					sortedQuads.begin(),
					sortedQuads.end(),
					[&cameraPosition]( const TransparentQuad& left, const TransparentQuad& right )
					{
						return DistanceSquared( left.position, cameraPosition ) >
							DistanceSquared( right.position, cameraPosition );
					} );

				const TextureHandle backbuffer = device.GetCurrentSwapchainTexture();
				RenderPass renderPass{};
				renderPass.color[ 0 ].loadOp = LoadOp::Clear;
				renderPass.color[ 0 ].clearColor = { 0.02f, 0.025f, 0.04f, 1.0f };
				renderPass.depthStencil.depthLoadOp = LoadOp::Clear;
				renderPass.depthStencil.clearDepth = 1.0f;

				Framebuffer framebuffer{};
				framebuffer.color[ 0 ].texture = backbuffer;
				framebuffer.depthStencil.texture = depthTarget.GetTexture();

				ICommandBuffer& commands = device.AcquireCommandBuffer();
				commands.CmdBeginRendering( renderPass, framebuffer );
				commands.CmdBindRenderPipeline( skyboxPipeline );
				commands.CmdPushConstants( &constants, sizeof( constants ) );
				commands.CmdDraw( 36 );

				//Opaque state: test depth, write depth, do not blend.
				commands.CmdPushDebugGroupLabel( "Opaque cube - depth write ON - blend OFF", 0xff2080ffu );
				commands.CmdBindRenderPipeline( opaquePipeline );
				commands.CmdBindVertexBuffer( cubeGeometry.vertexBuffer );
				commands.CmdBindIndexBuffer( cubeGeometry.indexBuffer );

				const XMMATRIX cubeModel =
					XMMatrixScaling( 0.85f, 0.85f, 0.85f ) *
					XMMatrixTranslation( 0.0f, -2.0f, 0.8f );
				XMStoreFloat4x4( &constants.model, XMMatrixTranspose( cubeModel ) );

				constants.color = { 1.0f, 0.55f, 0.05f, 1.0f };
				commands.CmdPushConstants( &constants, sizeof( constants ) );
				commands.CmdDrawIndexed( cubeGeometry.indexCount );
				commands.CmdPopDebugGroupLabel();

				// Transparent state: test depth, do not write depth, blend with the scene.
				commands.CmdPushDebugGroupLabel( "Transparent quads - depth write OFF - blend ON", 0xff40d080u );
				commands.CmdBindRenderPipeline( transparentPipeline );
				commands.CmdBindVertexBuffer( quadGeometry.vertexBuffer );
				commands.CmdBindIndexBuffer( quadGeometry.indexBuffer );

				for( const TransparentQuad& quad : sortedQuads )
				{
					const XMMATRIX model = XMMatrixScaling( 1.15f, 1.15f, 1.0f ) *
						XMMatrixTranslation( quad.position.x, quad.position.y, quad.position.z );
					XMStoreFloat4x4( &constants.model, XMMatrixTranspose( model ) );

					constants.color = quad.color;
					commands.CmdPushConstants( &constants, sizeof( constants ) );
					commands.CmdDrawIndexed( quadGeometry.indexCount );
				}
				commands.CmdPopDebugGroupLabel();

				commands.CmdEndRendering();
				device.Submit( commands, backbuffer );
			}

			device.WaitIdle();
		}

		utils::DestroyGeometry( device, quadGeometry );
		utils::DestroyGeometry( device, cubeGeometry );

		device.Destroy( cubeMap );

		transparentPipeline = {};
		opaquePipeline = {};
		skyboxPipeline = {};

		DeviceManager::ShutdownSingleton();
		return 0;
	}
	catch( const std::exception& error )
	{
		DeviceManager::ShutdownSingleton();
		MessageBoxA( nullptr, error.what(), "Ldx12 Transparency failed", MB_ICONERROR | MB_OK );
		return 1;
	}
}
