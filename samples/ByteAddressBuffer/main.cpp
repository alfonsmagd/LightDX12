#include "Ldx12/Ldx12.hpp"
#include "Ldx12/HLSLLoader.hpp"
#include "Ldx12Utils/AppLdx.hpp"
#include "Ldx12Utils/DepthTarget.hpp"

#include <DirectXMath.h>

#include <array>
#include <cstddef>
#include <chrono>
#include <cmath>
#include <numbers>
#include <exception>
#include <filesystem>

using namespace DirectX;
using namespace ldx12;

namespace
{
	struct Vertex
	{
		XMFLOAT3 position;
		XMFLOAT3 normal;
		uint32_t colorByteOffset = 0;
	};

	struct alignas( 16 ) SceneConstants
	{
		XMFLOAT4X4 modelViewProjection{};
		XMFLOAT4X4 model{};
		std::array<float, 4> lightColor = { 1.0f, 1.0f, 1.0f, 1.0f };
		XMFLOAT4 lightDirection{};
	};

	static_assert( sizeof( SceneConstants ) == 160 );

	// Clockwise faces with outward normals. Color offsets are assigned on the CPU.
	const std::array<Vertex, 36> kCubeVertices = {

		// Front (-Z).
		Vertex{ { -1.0f, -1.0f, -1.0f }, { 0.0f, 0.0f, -1.0f }, 0 },
		Vertex{ { 1.0f, 1.0f, -1.0f }, { 0.0f, 0.0f, -1.0f }, 0 },
		Vertex{ { 1.0f, -1.0f, -1.0f }, { 0.0f, 0.0f, -1.0f }, 0 },
		Vertex{ { -1.0f, -1.0f, -1.0f }, { 0.0f, 0.0f, -1.0f }, 0 },
		Vertex{ { -1.0f, 1.0f, -1.0f }, { 0.0f, 0.0f, -1.0f }, 0 },
		Vertex{ { 1.0f, 1.0f, -1.0f }, { 0.0f, 0.0f, -1.0f }, 0 },

		// Back (+Z).
		Vertex{ { -1.0f, -1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f }, 0 },
		Vertex{ { 1.0f, -1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f }, 0 },
		Vertex{ { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f }, 0 },
		Vertex{ { -1.0f, -1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f }, 0 },
		Vertex{ { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f }, 0 },
		Vertex{ { -1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f }, 0 },

		// Bottom (-Y).
		Vertex{ { -1.0f, -1.0f, 1.0f }, { 0.0f, -1.0f, 0.0f }, 0 },
		Vertex{ { 1.0f, -1.0f, -1.0f }, { 0.0f, -1.0f, 0.0f }, 0 },
		Vertex{ { 1.0f, -1.0f, 1.0f }, { 0.0f, -1.0f, 0.0f }, 0 },
		Vertex{ { -1.0f, -1.0f, 1.0f }, { 0.0f, -1.0f, 0.0f }, 0 },
		Vertex{ { -1.0f, -1.0f, -1.0f }, { 0.0f, -1.0f, 0.0f }, 0 },
		Vertex{ { 1.0f, -1.0f, -1.0f }, { 0.0f, -1.0f, 0.0f }, 0 },

		// Top (+Y).
		Vertex{ { -1.0f, 1.0f, -1.0f }, { 0.0f, 1.0f, 0.0f }, 0 },
		Vertex{ { 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f }, 0 },
		Vertex{ { 1.0f, 1.0f, -1.0f }, { 0.0f, 1.0f, 0.0f }, 0 },
		Vertex{ { -1.0f, 1.0f, -1.0f }, { 0.0f, 1.0f, 0.0f }, 0 },
		Vertex{ { -1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f }, 0 },
		Vertex{ { 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f }, 0 },

		// Right (+X).
		Vertex{ { 1.0f, -1.0f, -1.0f }, { 1.0f, 0.0f, 0.0f }, 0 },
		Vertex{ { 1.0f, 1.0f, 1.0f }, { 1.0f, 0.0f, 0.0f }, 0 },
		Vertex{ { 1.0f, -1.0f, 1.0f }, { 1.0f, 0.0f, 0.0f }, 0 },
		Vertex{ { 1.0f, -1.0f, -1.0f }, { 1.0f, 0.0f, 0.0f }, 0 },
		Vertex{ { 1.0f, 1.0f, -1.0f }, { 1.0f, 0.0f, 0.0f }, 0 },
		Vertex{ { 1.0f, 1.0f, 1.0f }, { 1.0f, 0.0f, 0.0f }, 0 },

		// Left (-X).
		Vertex{ { -1.0f, -1.0f, 1.0f }, { -1.0f, 0.0f, 0.0f }, 0 },
		Vertex{ { -1.0f, 1.0f, -1.0f }, { -1.0f, 0.0f, 0.0f }, 0 },
		Vertex{ { -1.0f, -1.0f, -1.0f }, { -1.0f, 0.0f, 0.0f }, 0 },
		Vertex{ { -1.0f, -1.0f, 1.0f }, { -1.0f, 0.0f, 0.0f }, 0 },
		Vertex{ { -1.0f, 1.0f, 1.0f }, { -1.0f, 0.0f, 0.0f }, 0 },
		Vertex{ { -1.0f, 1.0f, -1.0f }, { -1.0f, 0.0f, 0.0f }, 0 } };

	constexpr std::array<std::array<float, 4>, 6> kFaceColors = { std::array<float, 4>{ 0.12f, 0.55f, 1.00f, 1.0f },
		std::array<float, 4>{ 0.10f, 0.95f, 0.55f, 1.0f },
		std::array<float, 4>{ 0.85f, 0.20f, 1.00f, 1.0f },
		std::array<float, 4>{ 1.00f, 0.35f, 0.18f, 1.0f },
		std::array<float, 4>{ 1.00f, 0.80f, 0.15f, 1.0f },
		std::array<float, 4>{ 0.15f, 0.90f, 1.00f, 1.0f } };

	static_assert( sizeof( kFaceColors ) == 96 );
}

int WINAPI wWinMain( HINSTANCE instance, HINSTANCE, PWSTR, int showCommand )
{
	try
	{
		constexpr uint32_t kInitialWidth = 1280;
		constexpr uint32_t kInitialHeight = 800;
		constexpr float kLightCycleSeconds = 6.0f;
		constexpr float kViewDistance = 6.0f;
		constexpr float kNearPlane = 1.0f;
		constexpr float kFarPlane = 20.0f;
		const float verticalFieldOfView = 2.0f * std::atan( 1.0f / 2.25f );

		utils::AppLdxDesc appDesc{};
		appDesc.instance = instance;
		appDesc.showCommand = showCommand;
		appDesc.className = L"Ldx12ByteAddressBufferWindow";
		appDesc.title = L"Ldx12 ByteAddressBuffer - Rotating Cube";
		appDesc.width = kInitialWidth;
		appDesc.height = kInitialHeight;

		utils::AppLdx app( appDesc );
		HLSLLoader::SetRootDirectory( std::filesystem::path( __FILE__ ).parent_path() );

		ContextDesc contextDesc{};
		contextDesc.enableDebugLayer = true;

		SwapchainDesc swapchainDesc{};
		swapchainDesc.window = MakeWin32WindowHandle( app.GetWindow() );
		swapchainDesc.width = kInitialWidth;
		swapchainDesc.height = kInitialHeight;
		swapchainDesc.vsync = true;

		DeviceManager& deviceManager = DeviceManager::Initialize( contextDesc, swapchainDesc );
		app.SetDeviceManager( deviceManager );
		RenderDevice& device = *deviceManager.GetRenderDevice();

		RenderPipelineDesc pipelineDesc{};
		pipelineDesc.vertexShader = HLSLLoader::LoadStage( "shaders/ByteAddressBuffer.hlsl", "vs_6_6", "VSMain" );
		pipelineDesc.fragmentShader = HLSLLoader::LoadStage( "shaders/ByteAddressBuffer.hlsl", "ps_6_6", "PSMain" );
		pipelineDesc.color[ 0 ].format = contextDesc.swapchainFormat;
		pipelineDesc.depthFormat = DXGI_FORMAT_D32_FLOAT;
		pipelineDesc.inputElements[ 0 ].semanticName = "POSITION";
		pipelineDesc.inputElements[ 0 ].format = DXGI_FORMAT_R32G32B32_FLOAT;
		pipelineDesc.inputElements[ 0 ].alignedByteOffset = offsetof( Vertex, position );
		pipelineDesc.inputElements[ 1 ].semanticName = "NORMAL";
		pipelineDesc.inputElements[ 1 ].format = DXGI_FORMAT_R32G32B32_FLOAT;
		pipelineDesc.inputElements[ 1 ].alignedByteOffset = offsetof( Vertex, normal );
		pipelineDesc.inputElements[ 2 ].semanticName = "TEXCOORD";
		pipelineDesc.inputElements[ 2 ].format = DXGI_FORMAT_R32_UINT;
		pipelineDesc.inputElements[ 2 ].alignedByteOffset = offsetof( Vertex, colorByteOffset );
		pipelineDesc.rasterizerState.CullMode = D3D12_CULL_MODE_BACK;
		pipelineDesc.rasterizerState.FrontCounterClockwise = FALSE;
		pipelineDesc.depthStencilState.DepthEnable = TRUE;
		pipelineDesc.depthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		pipelineDesc.depthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		pipelineDesc.depthStencilState.StencilEnable = FALSE;

		RenderPipelineState pipeline = device.CreateRenderPipeline( pipelineDesc );

		std::array<Vertex, 36> vertices = kCubeVertices;

		for( uint32_t vertexIndex = 0; vertexIndex < vertices.size(); ++vertexIndex )
		{
			const uint32_t faceIndex = vertexIndex / 6;

			vertices[ vertexIndex ].colorByteOffset = faceIndex * sizeof( kFaceColors[ 0 ] );
		}

		BufferDesc vertexDesc{};
		vertexDesc.debugName = "ByteAddressBuffer Cube Vertices";
		vertexDesc.size = sizeof( vertices );
		vertexDesc.stride = sizeof( Vertex );
		vertexDesc.type = BufferType::Vertex;
		vertexDesc.initialData = vertices.data();

		const BufferHandle vertexBuffer = device.CreateBuffer( vertexDesc );

		BufferDesc sceneDesc{};
		sceneDesc.debugName = "ByteAddressBuffer Scene CBV";
		sceneDesc.size = sizeof( SceneConstants );
		sceneDesc.type = BufferType::Constant;

		const BufferHandle sceneBuffer = device.CreateBuffer( sceneDesc, ConstantBufferSlot::FreeCB0 );

		// Raw SRVs have no structured stride. The shader addresses colors in bytes.
		BufferDesc colorDesc{};
		colorDesc.debugName = "ByteAddressBuffer Face Colors";
		colorDesc.size = sizeof( kFaceColors );
		colorDesc.type = BufferType::Raw;
		colorDesc.initialData = kFaceColors.data();

		const BufferHandle colorBuffer = device.CreateBuffer( colorDesc, ShaderResourceSlot::FreeSRV0 );

		{
			utils::DepthTarget depthTarget( device );
			const auto animationStart = std::chrono::steady_clock::now();

			while( app.PumpMessages() )
			{
				if( app.IsWindowMinimized() )
				{
					WaitMessage();
					continue;
				}

				const float animationTime = std::chrono::duration<float>( std::chrono::steady_clock::now() - animationStart ).count();

				// Compute the white -> blue -> white transition entirely on the CPU.
				const float lightPhase = animationTime * 2.0f * std::numbers::pi_v<float> / kLightCycleSeconds;
				const float blueBlend = ( 1.0f - std::cos( lightPhase ) ) * 0.5f;
				const float redGreen = 1.0f - blueBlend;

				const float aspectRatio = static_cast<float>( deviceManager.GetWidth() ) / static_cast<float>( deviceManager.GetHeight() );
				const XMMATRIX model = XMMatrixRotationX( animationTime * 0.6f ) * XMMatrixRotationY( animationTime );
				const XMMATRIX view = XMMatrixTranslation( 0.0f, 0.0f, kViewDistance );
				const XMMATRIX projection = XMMatrixPerspectiveFovLH( verticalFieldOfView, aspectRatio, kNearPlane, kFarPlane );
				const XMVECTOR lightDirection = XMVector3Normalize( XMVectorSet( -0.35f, 0.8f, -0.45f, 0.0f ) );

				SceneConstants scene{};
				XMStoreFloat4x4( &scene.modelViewProjection, XMMatrixTranspose( model * view * projection ) );
				XMStoreFloat4x4( &scene.model, XMMatrixTranspose( model ) );
				XMStoreFloat4( &scene.lightDirection, lightDirection );
				scene.lightColor = { redGreen, redGreen, 1.0f, 1.0f };

				device.WriteBuffer( sceneBuffer, 0, &scene, sizeof( scene ) );
				depthTarget.Resize( deviceManager.GetWidth(), deviceManager.GetHeight() );

				CommandBuffer& commands = device.AcquireCommandBuffer();
				const TextureHandle backbuffer = device.GetCurrentSwapchainTexture();

				RenderPass renderPass{};
				renderPass.color[ 0 ].loadOp = LoadOp::Clear;
				renderPass.color[ 0 ].clearColor = { 0.035f, 0.045f, 0.065f, 1.0f };
				renderPass.depthStencil.depthLoadOp = LoadOp::Clear;
				renderPass.depthStencil.clearDepth = 1.0f;

				Framebuffer framebuffer{};
				framebuffer.color[ 0 ].texture = backbuffer;
				framebuffer.depthStencil.texture = depthTarget.GetTexture();

				commands.CmdBeginRendering( renderPass, framebuffer );
				commands.CmdBindRenderPipeline( pipeline );
				commands.CmdBindVertexBuffer( vertexBuffer );
				commands.CmdDraw( 36 );
				commands.CmdEndRendering();
				device.Submit( commands, backbuffer );
			}

			deviceManager.WaitIdle();
		}

		device.Destroy( vertexBuffer );
		device.Destroy( colorBuffer );
		device.Destroy( sceneBuffer );
		pipeline = {};
		DeviceManager::ShutdownSingleton();

		return 0;
	}
	catch( const std::exception& error )
	{
		DeviceManager::ShutdownSingleton();
		MessageBoxA( nullptr, error.what(), "Ldx12 ByteAddressBuffer failed", MB_ICONERROR | MB_OK );

		return 1;
	}
}
