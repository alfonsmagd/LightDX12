#include "Ldx12/Ldx12.hpp"
#include "Ldx12/HLSLLoader.hpp"
#include "Ldx12Utils/AppLdx.hpp"
#include "Ldx12Utils/DepthTarget.hpp"

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <filesystem>

using namespace ldx12;

namespace
{
	constexpr uint32_t kCubeColumns = 8;
	constexpr uint32_t kCubeRows = 4;
	constexpr uint32_t kCubeCount = kCubeColumns * kCubeRows;
	constexpr float kCubeSpacing = 1.7f;
	constexpr float kCubeScale = 0.38f;
	constexpr float kViewDistance = 18.0f;

	struct alignas( 16 ) MatrixRows
	{
		std::array<float, 4> row0 = { 1.0f, 0.0f, 0.0f, 0.0f };
		std::array<float, 4> row1 = { 0.0f, 1.0f, 0.0f, 0.0f };
		std::array<float, 4> row2 = { 0.0f, 0.0f, 1.0f, 0.0f };
		std::array<float, 4> row3 = { 0.0f, 0.0f, 0.0f, 1.0f };
	};

	static_assert( sizeof( MatrixRows ) == 64 );

	struct alignas( 16 ) SceneConstants
	{
		float aspectRatio = 1.0f;
		float viewDistance = kViewDistance;
		std::array<float, 2> padding = {};
		std::array<float, 4> lightDirection = { -0.35f, 0.8f, -0.45f, 0.0f };
	};

	static_assert( sizeof( SceneConstants ) == 32 );

	struct alignas( 16 ) CubeData
	{
		MatrixRows model;
		std::array<float, 4> color = {};
	};

	static_assert( sizeof( CubeData ) == 80 );

	struct Vec3
	{
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
	};

	struct GraphicsState
	{
		DeviceManager* deviceManager = nullptr;
		RenderPipelineState pipeline;
		BufferHandle sceneBuffer = {};
		BufferHandle cubeBuffer = {};
	};

	constexpr std::array<std::array<float, 4>, 8> kCubeColors = { std::array<float, 4>{ 0.12f, 0.55f, 1.00f, 1.0f },
		std::array<float, 4>{ 0.10f, 0.95f, 0.55f, 1.0f },
		std::array<float, 4>{ 0.85f, 0.20f, 1.00f, 1.0f },
		std::array<float, 4>{ 1.00f, 0.35f, 0.18f, 1.0f },
		std::array<float, 4>{ 1.00f, 0.80f, 0.15f, 1.0f },
		std::array<float, 4>{ 0.15f, 0.90f, 1.00f, 1.0f },
		std::array<float, 4>{ 0.45f, 0.30f, 1.00f, 1.0f },
		std::array<float, 4>{ 1.00f, 0.25f, 0.65f, 1.0f } };

	Vec3 RotateX( Vec3 value, float angle )
	{
		const float cosine = std::cos( angle );
		const float sine = std::sin( angle );
		return { value.x, value.y * cosine - value.z * sine, value.y * sine + value.z * cosine };
	}

	Vec3 RotateY( Vec3 value, float angle )
	{
		const float cosine = std::cos( angle );
		const float sine = std::sin( angle );
		return { value.x * cosine + value.z * sine, value.y, -value.x * sine + value.z * cosine };
	}

	Vec3 RotateZ( Vec3 value, float angle )
	{
		const float cosine = std::cos( angle );
		const float sine = std::sin( angle );
		return { value.x * cosine - value.y * sine, value.x * sine + value.y * cosine, value.z };
	}

	Vec3 RotateXYZ( Vec3 value, float angleX, float angleY, float angleZ )
	{
		return RotateZ( RotateY( RotateX( value, angleX ), angleY ), angleZ );
	}

	MatrixRows BuildCubeMatrix( uint32_t cubeIndex, float animationTime )
	{
		const uint32_t column = cubeIndex % kCubeColumns;
		const uint32_t row = cubeIndex / kCubeColumns;
		const float x = ( static_cast<float>( column ) - static_cast<float>( kCubeColumns - 1 ) * 0.5f ) * kCubeSpacing;
		const float z = ( static_cast<float>( row ) - static_cast<float>( kCubeRows - 1 ) * 0.5f ) * kCubeSpacing;
		const float y = std::sin( animationTime * 1.25f + static_cast<float>( cubeIndex ) * 0.41f ) * 0.45f;
		const float angle = animationTime + static_cast<float>( cubeIndex ) * 0.19f;

		const Vec3 basisX = RotateXYZ( { kCubeScale, 0.0f, 0.0f }, angle * 0.6f, angle, angle * 0.35f );
		const Vec3 basisY = RotateXYZ( { 0.0f, kCubeScale, 0.0f }, angle * 0.6f, angle, angle * 0.35f );
		const Vec3 basisZ = RotateXYZ( { 0.0f, 0.0f, kCubeScale }, angle * 0.6f, angle, angle * 0.35f );

		MatrixRows matrix{};
		matrix.row0 = { basisX.x, basisY.x, basisZ.x, x };
		matrix.row1 = { basisX.y, basisY.y, basisZ.y, y };
		matrix.row2 = { basisX.z, basisY.z, basisZ.z, z };
		matrix.row3 = { 0.0f, 0.0f, 0.0f, 1.0f };
		return matrix;
	}

	std::array<CubeData, kCubeCount> BuildCubeData( float animationTime )
	{
		std::array<CubeData, kCubeCount> cubes{};
		for( uint32_t cubeIndex = 0; cubeIndex < kCubeCount; ++cubeIndex )
		{
			cubes[ cubeIndex ].model = BuildCubeMatrix( cubeIndex, animationTime );
			cubes[ cubeIndex ].color = kCubeColors[ cubeIndex % kCubeColors.size() ];
		}
		return cubes;
	}

	RenderPipelineState CreatePipeline( RenderDevice& device, DXGI_FORMAT colorFormat, DXGI_FORMAT depthFormat )
	{
		RenderPipelineDesc desc{};
		desc.vertexShader = HLSLLoader::LoadStage( "shaders/CBSRVCubes.hlsl", "vs_6_6", "VSMain" );
		desc.fragmentShader = HLSLLoader::LoadStage( "shaders/CBSRVCubes.hlsl", "ps_6_6", "PSMain" );
		desc.color[ 0 ].format = colorFormat;
		desc.depthFormat = depthFormat;
		desc.rasterizerState.CullMode = D3D12_CULL_MODE_BACK;
		desc.depthStencilState.DepthEnable = TRUE;
		desc.depthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		desc.depthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		desc.depthStencilState.StencilEnable = FALSE;
		return device.CreateRenderPipeline( desc );
	}

	void UpdateSceneCbv( GraphicsState& gfx )
	{
		RenderDevice& device = *gfx.deviceManager->GetRenderDevice();
		if( !gfx.sceneBuffer.Valid() )
		{
			BufferDesc desc{};
			desc.debugName = "CB SRV Cubes Scene CBV";
			desc.size = sizeof( SceneConstants );
			desc.type = BufferType::Constant;
			gfx.sceneBuffer = device.CreateBuffer( desc, ConstantBufferSlot::FreeCB0 );
		}

		SceneConstants scene{};
		scene.aspectRatio = static_cast<float>( gfx.deviceManager->GetWidth() ) / static_cast<float>( gfx.deviceManager->GetHeight() );
		device.WriteBuffer( gfx.sceneBuffer, 0, &scene, sizeof( scene ) );
	}

	void UpdateCubeSrv( GraphicsState& gfx, float animationTime )
	{
		RenderDevice& device = *gfx.deviceManager->GetRenderDevice();
		if( !gfx.cubeBuffer.Valid() )
		{
			BufferDesc desc{};
			desc.debugName = "CB SRV Cubes Data SRV";
			desc.size = sizeof( CubeData ) * kCubeCount;
			desc.stride = sizeof( CubeData );
			desc.type = BufferType::Structured;
			gfx.cubeBuffer = device.CreateBuffer( desc, ShaderResourceSlot::FreeSRV0 );
		}

		const std::array<CubeData, kCubeCount> cubes = BuildCubeData( animationTime );
		device.WriteBuffer( gfx.cubeBuffer, 0, cubes.data(), sizeof( cubes ) );
	}

	void DestroyBuffer( RenderDevice& device, BufferHandle& buffer )
	{
		if( buffer.Valid() )
		{
			device.Destroy( buffer );
			buffer = {};
		}
	}

}

int WINAPI wWinMain( HINSTANCE instance, HINSTANCE, PWSTR, int showCommand )
{
	try
	{
		constexpr uint32_t kInitialWidth = 1280;
		constexpr uint32_t kInitialHeight = 800;
		utils::AppLdxDesc appDesc{};
		appDesc.instance = instance;
		appDesc.showCommand = showCommand;
		appDesc.className = L"Ldx12CBSRVCubesWindow";
		appDesc.title = L"Ldx12 CBV + SRV Cubes";
		appDesc.width = kInitialWidth;
		appDesc.height = kInitialHeight;
		utils::AppLdx app( appDesc );

		GraphicsState gfx{};
		HLSLLoader::SetRootDirectory( std::filesystem::path( __FILE__ ).parent_path() );

		ContextDesc contextDesc{};
		contextDesc.enableDebugLayer = true;

		SwapchainDesc swapchainDesc{};
		swapchainDesc.window = MakeWin32WindowHandle( app.GetWindow() );
		swapchainDesc.width = kInitialWidth;
		swapchainDesc.height = kInitialHeight;
		swapchainDesc.vsync = true;

		gfx.deviceManager = &DeviceManager::Initialize( contextDesc, swapchainDesc );
		app.SetDeviceManager( *gfx.deviceManager );
		RenderDevice& device = *gfx.deviceManager->GetRenderDevice();
		gfx.pipeline = CreatePipeline( device, contextDesc.swapchainFormat, DXGI_FORMAT_D32_FLOAT );
		{
			utils::DepthTarget depthTarget( device );
			const std::chrono::steady_clock::time_point animationStart = std::chrono::steady_clock::now();
			while( app.PumpMessages() )
			{
				if( app.IsWindowMinimized() )
				{
					WaitMessage();
					continue;
				}

				const float animationTime = std::chrono::duration<float>( std::chrono::steady_clock::now() - animationStart ).count();
				depthTarget.Resize( gfx.deviceManager->GetWidth(), gfx.deviceManager->GetHeight() );
				UpdateSceneCbv( gfx );
				UpdateCubeSrv( gfx, animationTime );

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
				commands.CmdBindRenderPipeline( gfx.pipeline );
				commands.CmdDraw( 36, kCubeCount );
				commands.CmdEndRendering();
				device.Submit( commands, backbuffer );
			}

			gfx.deviceManager->WaitIdle();
		}

		DestroyBuffer( device, gfx.sceneBuffer );
		DestroyBuffer( device, gfx.cubeBuffer );
		gfx.pipeline = {};
		DeviceManager::ShutdownSingleton();
		gfx.deviceManager = nullptr;
		return 0;
	}
	catch( const std::exception& error )
	{
		DeviceManager::ShutdownSingleton();
		MessageBoxA( nullptr, error.what(), "Ldx12 CBV + SRV Cubes failed", MB_ICONERROR | MB_OK );
		return 1;
	}
}
