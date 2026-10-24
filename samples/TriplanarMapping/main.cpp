#include "Ldx12/HLSLLoader.hpp"
#include "Ldx12/Ldx12.hpp"
#include "Ldx12Utils/AppLdx.hpp"
#include "Ldx12Utils/DepthTarget.hpp"
#include "Ldx12Utils/TextureLoader.hpp"

#include <DirectXMath.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <random>
#include <vector>

using namespace DirectX;
using namespace ldx12;

namespace
{
	constexpr uint32_t ourObjectCount = 16;
	constexpr uint32_t ourFramesInFlight = 3;
	constexpr uint32_t ourTextureSize = 256;
	constexpr float ourGoldenRatio = 1.6180339887498948482f;

	struct Vertex
	{
		XMFLOAT3 position{};
		XMFLOAT3 color{};
	};

	struct PushConstants
	{
		XMFLOAT4X4 viewProjection{};
		uint32_t modelBufferIndex = 0;
		uint32_t texture0Index = 0;
		uint32_t texture1Index = 0;
		uint32_t samplerIndex = 0;
	};

	static_assert( sizeof( Vertex ) == 24 );
	static_assert( sizeof( PushConstants ) == 80 );

	constexpr std::array<Vertex, 12> ourVertices = {
		Vertex{ { -1.0f, ourGoldenRatio, 0.0f }, { 0.0f, 1.0f, 0.0f } },
		Vertex{ { 1.0f, ourGoldenRatio, 0.0f }, { 1.0f, 1.0f, 0.0f } },
		Vertex{ { -1.0f, -ourGoldenRatio, 0.0f }, { 0.0f, 1.0f, 0.0f } },
		Vertex{ { 1.0f, -ourGoldenRatio, 0.0f }, { 1.0f, 1.0f, 0.0f } },
		Vertex{ { 0.0f, -1.0f, ourGoldenRatio }, { 0.0f, 0.0f, 1.0f } },
		Vertex{ { 0.0f, 1.0f, ourGoldenRatio }, { 0.0f, 1.0f, 1.0f } },
		Vertex{ { 0.0f, -1.0f, -ourGoldenRatio }, { 0.0f, 0.0f, 1.0f } },
		Vertex{ { 0.0f, 1.0f, -ourGoldenRatio }, { 0.0f, 1.0f, 1.0f } },
		Vertex{ { ourGoldenRatio, 0.0f, -1.0f }, { 1.0f, 0.0f, 0.0f } },
		Vertex{ { ourGoldenRatio, 0.0f, 1.0f }, { 1.0f, 0.0f, 1.0f } },
		Vertex{ { -ourGoldenRatio, 0.0f, -1.0f }, { 1.0f, 0.0f, 0.0f } },
		Vertex{ { -ourGoldenRatio, 0.0f, 1.0f }, { 1.0f, 0.0f, 1.0f } },
	};

	constexpr std::array<uint16_t, 60> ourIndices = { 0,
		11,
		5,
		0,
		5,
		1,
		0,
		1,
		7,
		0,
		7,
		10,
		0,
		10,
		11,
		1,
		5,
		9,
		5,
		11,
		4,
		11,
		10,
		2,
		10,
		7,
		6,
		7,
		1,
		8,
		3,
		9,
		4,
		3,
		4,
		2,
		3,
		2,
		6,
		3,
		6,
		8,
		3,
		8,
		9,
		4,
		9,
		5,
		2,
		4,
		11,
		6,
		2,
		10,
		8,
		6,
		7,
		9,
		8,
		1 };

	RenderPipelineState CreatePipeline( RenderDevice& device, DXGI_FORMAT colorFormat )
	{
		RenderPipelineDesc desc{};
		desc.vertexShader = HLSLLoader::LoadStage( "shaders/TriplanarMapping.hlsl", "vs_6_6", "VSMain" );
		desc.fragmentShader = HLSLLoader::LoadStage( "shaders/TriplanarMapping.hlsl", "ps_6_6", "PSMain" );
		desc.color[ 0 ].format = colorFormat;
		desc.depthFormat = DXGI_FORMAT_D32_FLOAT;
		desc.inputElements[ 0 ].semanticName = "POSITION";
		desc.inputElements[ 0 ].format = DXGI_FORMAT_R32G32B32_FLOAT;
		desc.inputElements[ 1 ].semanticName = "COLOR";
		desc.inputElements[ 1 ].format = DXGI_FORMAT_R32G32B32_FLOAT;
		desc.inputElements[ 1 ].alignedByteOffset = sizeof( XMFLOAT3 );
		desc.rasterizerState.CullMode = D3D12_CULL_MODE_BACK;
		desc.depthStencilState.DepthEnable = TRUE;
		desc.depthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		desc.depthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		desc.depthStencilState.StencilEnable = FALSE;

		return device.CreateRenderPipeline( desc );
	}

	BufferHandle CreateVertexBuffer( RenderDevice& device )
	{
		BufferDesc desc{};
		desc.debugName = "Triplanar icosahedron vertices";
		desc.size = sizeof( ourVertices );
		desc.stride = sizeof( Vertex );
		desc.type = BufferType::Vertex;
		desc.initialData = ourVertices.data();

		return device.CreateBuffer( desc );
	}

	BufferHandle CreateIndexBuffer( RenderDevice& device )
	{
		BufferDesc desc{};
		desc.debugName = "Triplanar icosahedron indices";
		desc.size = sizeof( ourIndices );
		desc.stride = sizeof( uint16_t );
		desc.type = BufferType::Index;
		desc.initialData = ourIndices.data();

		return device.CreateBuffer( desc );
	}

	BufferHandle CreateModelBuffer( RenderDevice& device )
	{
		BufferDesc desc{};
		desc.debugName = "Triplanar model matrices";
		desc.size = sizeof( XMFLOAT4X4 ) * ourObjectCount;
		desc.stride = sizeof( XMFLOAT4X4 );
		desc.type = BufferType::Structured;
		desc.memory = BufferMemory::CpuToGpu;

		return device.CreateBuffer( desc );
	}

	TextureHandle CreateRgbaTexture( RenderDevice& device, const char* debugName, const std::vector<uint32_t>& pixels )
	{
		TextureDesc desc{};
		desc.debugName = debugName;
		desc.width = ourTextureSize;
		desc.height = ourTextureSize;
		desc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.data = pixels.data();
		desc.rowPitch = ourTextureSize * sizeof( uint32_t );
		desc.slicePitch = desc.rowPitch * ourTextureSize;

		return device.CreateTexture( desc );
	}

	TextureHandle CreateXorTexture( RenderDevice& device )
	{
		std::vector<uint32_t> pixels( ourTextureSize * ourTextureSize );

		for( uint32_t y = 0; y < ourTextureSize; ++y )
		{
			for( uint32_t x = 0; x < ourTextureSize; ++x )
			{
				const uint32_t value = x ^ y;
				pixels[ static_cast<size_t>( y ) * ourTextureSize + x ] = 0xff000000u | value << 16u | value << 8u | value;
			}
		}

		return CreateRgbaTexture( device, "Triplanar XOR pattern", pixels );
	}

	TextureHandle CreateProceduralWoodTexture( RenderDevice& device )
	{
		std::vector<uint32_t> pixels( ourTextureSize * ourTextureSize );

		for( uint32_t y = 0; y < ourTextureSize; ++y )
		{
			for( uint32_t x = 0; x < ourTextureSize; ++x )
			{
				const float fx = static_cast<float>( x ) / static_cast<float>( ourTextureSize );
				const float fy = static_cast<float>( y ) / static_cast<float>( ourTextureSize );
				const float grain = 0.5f + 0.5f * std::sin( fx * 70.0f + std::sin( fy * 18.0f ) * 4.0f );
				const uint32_t red = static_cast<uint32_t>( 72.0f + grain * 86.0f );
				const uint32_t green = static_cast<uint32_t>( 35.0f + grain * 48.0f );
				const uint32_t blue = static_cast<uint32_t>( 14.0f + grain * 24.0f );
				pixels[ static_cast<size_t>( y ) * ourTextureSize + x ] = 0xff000000u | blue << 16u | green << 8u | red;
			}
		}

		return CreateRgbaTexture( device, "Triplanar procedural wood", pixels );
	}

	TextureHandle CreateWoodTexture( RenderDevice& device )
	{
		const std::filesystem::path woodPath = std::filesystem::path( LDX12_MEDIA_DIRECTORY ) / "bistro" / "BuildingTextures" / "wood_polished_01_diff.png";

		if( !std::filesystem::exists( woodPath ) )
		{
			return CreateProceduralWoodTexture( device );
		}

		const utils::ImageRgba8 image = utils::LoadImageRgba8( woodPath );
		TextureDesc desc{};
		desc.debugName = "wood_polished_01_diff.png";
		desc.width = image.width;
		desc.height = image.height;
		desc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.data = image.pixels.data();
		desc.rowPitch = image.width * 4u;
		desc.slicePitch = desc.rowPitch * image.height;

		return device.CreateTexture( desc );
	}

	std::array<XMFLOAT3, ourObjectCount> CreateRotationAxes()
	{
		std::array<XMFLOAT3, ourObjectCount> axes{};
		std::mt19937 random( 42u );
		std::uniform_real_distribution<float> component( -1.0f, 1.0f );

		for( XMFLOAT3& axis : axes )
		{
			XMVECTOR value{};

			do
			{
				value = XMVectorSet( component( random ), component( random ), component( random ), 0.0f );
			} while( XMVectorGetX( XMVector3LengthSq( value ) ) < 0.001f );

			XMStoreFloat3( &axis, XMVector3Normalize( value ) );
		}

		return axes;
	}

	void UpdateModelMatrices( RenderDevice& device, BufferHandle modelBuffer, const std::array<XMFLOAT3, ourObjectCount>& axes, float time )
	{
		std::array<XMFLOAT4X4, ourObjectCount> matrices{};
		constexpr uint32_t objectsPerLine = 4;

		for( uint32_t index = 0; index < ourObjectCount; ++index )
		{
			const float direction = index % 2u == 0u ? -1.0f : 1.0f;
			const float x = -6.0f + 4.0f * static_cast<float>( index % objectsPerLine );
			const float y = -6.0f + 4.0f * static_cast<float>( index / objectsPerLine );
			const XMVECTOR axis = XMLoadFloat3( &axes[ index ] );
			const XMMATRIX model = XMMatrixRotationAxis( axis, direction * time ) * XMMatrixTranslation( x, y, 0.0f );

			XMStoreFloat4x4( &matrices[ index ], XMMatrixTranspose( model ) );
		}

		device.WriteBuffer( modelBuffer, 0, matrices.data(), sizeof( matrices ) );
	}

	PushConstants BuildPushConstants( RenderDevice& device, BufferHandle modelBuffer, TextureHandle texture0, TextureHandle texture1, float aspectRatio )
	{
		const XMMATRIX view = XMMatrixTranslation( 0.0f, 0.0f, 14.0f * ourGoldenRatio );
		const XMMATRIX projection = XMMatrixPerspectiveFovLH( XMConvertToRadians( 45.0f ), aspectRatio, 0.1f, 100.0f );

		PushConstants constants{};
		XMStoreFloat4x4( &constants.viewProjection, XMMatrixTranspose( view * projection ) );
		constants.modelBufferIndex = device.GetBindlessIndex( modelBuffer );
		constants.texture0Index = device.GetBindlessIndex( texture0 );
		constants.texture1Index = device.GetBindlessIndex( texture1 );
		constants.samplerIndex = ToSamplerIndex( SamplerSlot::LinearWrap );

		return constants;
	}
}

int WINAPI wWinMain( HINSTANCE instance, HINSTANCE, PWSTR, int showCommand )
{
	try
	{
		constexpr uint32_t initialWidth = 1280;
		constexpr uint32_t initialHeight = 1024;

		HLSLLoader::SetRootDirectory( std::filesystem::path( __FILE__ ).parent_path() );

		utils::AppLdxDesc appDesc{};
		appDesc.instance = instance;
		appDesc.showCommand = showCommand;
		appDesc.className = L"Ldx12TriplanarMappingWindow";
		appDesc.title = L"Ldx12 - Triplanar Mapping";
		appDesc.width = initialWidth;
		appDesc.height = initialHeight;

		utils::AppLdx app( appDesc );

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

		RenderPipelineState pipeline = CreatePipeline( device, context.swapchainFormat );
		BufferHandle vertexBuffer = CreateVertexBuffer( device );
		BufferHandle indexBuffer = CreateIndexBuffer( device );
		std::array<BufferHandle, ourFramesInFlight> modelBuffers{};

		for( BufferHandle& modelBuffer : modelBuffers )
		{
			modelBuffer = CreateModelBuffer( device );
		}

		TextureHandle xorTexture = CreateXorTexture( device );
		TextureHandle woodTexture = CreateWoodTexture( device );
		utils::DepthTarget depthTarget( device );
		const std::array<XMFLOAT3, ourObjectCount> rotationAxes = CreateRotationAxes();
		const std::chrono::steady_clock::time_point animationStart = std::chrono::steady_clock::now();
		std::array<SubmitHandle, ourFramesInFlight> frameSubmissions{};
		uint32_t frameIndex = 0;

		while( app.PumpMessages() )
		{
			if( app.IsWindowMinimized() )
			{
				WaitMessage();
				continue;
			}

			// Reuse matrices only after the GPU has finished this frame slot.
			device.Wait( frameSubmissions[ frameIndex ] );

			const BufferHandle modelBuffer = modelBuffers[ frameIndex ];
			const uint32_t width = manager.GetWidth();
			const uint32_t height = manager.GetHeight();

			depthTarget.Resize( width, height );

			const float time = std::chrono::duration<float>( std::chrono::steady_clock::now() - animationStart ).count();

			UpdateModelMatrices( device, modelBuffer, rotationAxes, time );
			const PushConstants constants =
				BuildPushConstants( device, modelBuffer, xorTexture, woodTexture, static_cast<float>( width ) / static_cast<float>( height ) );

			const TextureHandle backbuffer = device.GetCurrentSwapchainTexture();

			RenderPass renderPass{};
			renderPass.color[ 0 ].loadOp = LoadOp::Clear;
			renderPass.color[ 0 ].clearColor = { 1.0f, 1.0f, 1.0f, 1.0f };
			renderPass.depthStencil.depthLoadOp = LoadOp::Clear;
			renderPass.depthStencil.clearDepth = 1.0f;

			Framebuffer framebuffer{};
			framebuffer.color[ 0 ].texture = backbuffer;
			framebuffer.depthStencil.texture = depthTarget.GetTexture();

			CommandBuffer& commands = device.AcquireCommandBuffer();
			commands.CmdBeginRendering( renderPass, framebuffer );
			commands.CmdBindRenderPipeline( pipeline );
			commands.CmdBindVertexBuffer( vertexBuffer, sizeof( Vertex ) );
			commands.CmdBindIndexBuffer( indexBuffer, DXGI_FORMAT_R16_UINT );
			commands.CmdPushConstants( &constants, sizeof( constants ) );
			commands.CmdDrawIndexed( static_cast<uint32_t>( ourIndices.size() ), ourObjectCount );
			commands.CmdEndRendering();
			frameSubmissions[ frameIndex ] = device.Submit( commands, backbuffer );
			frameIndex = ( frameIndex + 1u ) % ourFramesInFlight;
		}

		device.WaitIdle();
		depthTarget.Reset();
		device.Destroy( woodTexture );
		device.Destroy( xorTexture );

		for( BufferHandle modelBuffer : modelBuffers )
		{
			device.Destroy( modelBuffer );
		}

		device.Destroy( indexBuffer );
		device.Destroy( vertexBuffer );
		pipeline = {};
		DeviceManager::ShutdownSingleton();
		return 0;
	}
	catch( const std::exception& error )
	{
		DeviceManager::ShutdownSingleton();
		MessageBoxA( nullptr, error.what(), "Ldx12 Triplanar Mapping failed", MB_ICONERROR | MB_OK );
		return 1;
	}
}
