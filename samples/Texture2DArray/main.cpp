#include "Ldx12/Ldx12.hpp"
#include "Ldx12Utils/AppLdx.hpp"

#include <DirectXMath.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <exception>
#include <random>

using namespace ldx12;

namespace
{
	constexpr uint32_t ourTextureSize = 64;
	constexpr uint32_t ourLayerCount = 4;

	struct PushConstants
	{
		uint32_t textureIndex = 0;
		uint32_t samplerIndex = 0;
		uint32_t layer = 0;
		float scale = 0.25f;
		DirectX::XMFLOAT2 position = {};
	};

	struct MovingTriangle
	{
		DirectX::XMFLOAT2 position = {};
		DirectX::XMFLOAT2 velocity = {};
		uint32_t layer = 0;
	};

	struct GraphicsState
	{
		DeviceManager* deviceManager = nullptr;
		RenderPipelineState pipeline;
		TextureHandle textureArray = {};
	};

	RenderPipelineState CreatePipeline( RenderDevice& device )
	{
		static constexpr char vertexShader[] = R"(
cbuffer PushConstants : register(b0)
{
    uint gTextureIndex;
    uint gSamplerIndex;
    uint gLayer;
    float gScale;
    float2 gPosition;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

VertexOutput VSMain(uint vertexId : SV_VertexID)
{
    const float2 positions[3] =
    {
        float2( 0.0,  0.75),
        float2( 0.70, -0.65),
        float2(-0.70, -0.65)
    };

    const float2 uvs[3] =
    {
        float2(0.5, 0.0),
        float2(1.0, 1.0),
        float2(0.0, 1.0)
    };

    VertexOutput output;
    output.position = float4(positions[vertexId] * gScale + gPosition, 0.0, 1.0);
    output.uv = uvs[vertexId];
    return output;
}
)";

		static constexpr char pixelShader[] = R"(
cbuffer PushConstants : register(b0)
{
    uint gTextureIndex;
    uint gSamplerIndex;
    uint gLayer;
    float gScale;
    float2 gPosition;
};

float4 PSMain(float4 position : SV_Position, float2 uv : TEXCOORD0) : SV_Target0
{
    Texture2DArray<float4> textures = ResourceDescriptorHeap[gTextureIndex];
    SamplerState textureSampler = SamplerDescriptorHeap[gSamplerIndex];
    return textures.Sample(textureSampler, float3(uv, float(gLayer)));
}
)";

		RenderPipelineDesc desc{};
		desc.vertexShader.source = vertexShader;
		desc.vertexShader.entryPoint = "VSMain";
		desc.vertexShader.profile = "vs_6_6";
		desc.fragmentShader.source = pixelShader;
		desc.fragmentShader.entryPoint = "PSMain";
		desc.fragmentShader.profile = "ps_6_6";
		desc.colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.depthFormat = DXGI_FORMAT_UNKNOWN;
		desc.depthStencilState.DepthEnable = FALSE;
		desc.depthStencilState.StencilEnable = FALSE;
		return device.CreateRenderPipeline( desc );
	}

	TextureHandle CreateTextureArray( RenderDevice& device )
	{
		constexpr uint32_t layerColors[ ourLayerCount ] = { 0xff0000ffu, 0xff00ff00u, 0xffff0000u, 0xff00ffffu };
		constexpr uint32_t layerSecondColors[ ourLayerCount ] = { 0xffffffffu, 0xff101010u, 0xffffffffu, 0xff101010u };

		std::array<uint32_t, ourTextureSize * ourTextureSize * ourLayerCount> pixels{};
		for( uint32_t layer = 0; layer < ourLayerCount; ++layer )
		{
			for( uint32_t y = 0; y < ourTextureSize; ++y )
			{
				for( uint32_t x = 0; x < ourTextureSize; ++x )
				{
					const bool firstColor = ( ( x / 8u ) + ( y / 8u ) ) % 2u == 0u;
					const uint32_t pixelIndex = layer * ourTextureSize * ourTextureSize + y * ourTextureSize + x;
					pixels[ pixelIndex ] = firstColor ? layerColors[ layer ] : layerSecondColors[ layer ];
				}
			}
		}

		TextureDesc desc{};
		desc.debugName = "Triangle Texture2DArray";
		desc.width = ourTextureSize;
		desc.height = ourTextureSize;
		desc.depthOrArraySize = ourLayerCount;
		desc.dimension = TextureDimension::Texture2DArray;
		desc.data = pixels.data();
		desc.rowPitch = ourTextureSize * sizeof( uint32_t );
		desc.slicePitch = ourTextureSize * ourTextureSize * sizeof( uint32_t );
		return device.CreateTexture( desc );
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
		appDesc.className = L"Ldx12Texture2DArrayWindow";
		appDesc.title = L"Ldx12 Texture2DArray - Four moving layers";
		appDesc.width = initialWidth;
		appDesc.height = initialHeight;
		utils::AppLdx app( appDesc );

		GraphicsState gfx{};
		ContextDesc context{};
		context.enableDebugLayer = true;

		SwapchainDesc swapchain{};
		swapchain.window = MakeWin32WindowHandle( app.GetWindow() );
		swapchain.width = initialWidth;
		swapchain.height = initialHeight;
		swapchain.vsync = true;

		gfx.deviceManager = &DeviceManager::Initialize( context, swapchain );
		app.SetDeviceManager( *gfx.deviceManager );

		RenderDevice& device = *gfx.deviceManager->GetRenderDevice();
		gfx.pipeline = CreatePipeline( device );
		gfx.textureArray = CreateTextureArray( device );

		std::random_device randomDevice;
		std::mt19937 randomGenerator( randomDevice() );
		std::uniform_real_distribution<float> positionDistribution( -0.65f, 0.65f );
		std::uniform_real_distribution<float> angleDistribution( 0.0f, DirectX::XM_2PI );
		std::uniform_real_distribution<float> speedDistribution( 0.25f, 0.45f );

		std::array<MovingTriangle, ourLayerCount> triangles{};
		for( uint32_t layer = 0; layer < ourLayerCount; ++layer )
		{
			const float angle = angleDistribution( randomGenerator );
			const float speed = speedDistribution( randomGenerator );
			triangles[ layer ].position = { positionDistribution( randomGenerator ), positionDistribution( randomGenerator ) };
			triangles[ layer ].velocity = { std::cos( angle ) * speed, std::sin( angle ) * speed };
			triangles[ layer ].layer = layer;
		}

		const auto updateTriangles = []( std::array<MovingTriangle, ourLayerCount>& movingTriangles, float deltaTime )
		{
			for( MovingTriangle& triangle : movingTriangles )
			{
				triangle.position.x += triangle.velocity.x * deltaTime;
				triangle.position.y += triangle.velocity.y * deltaTime;

				if( triangle.position.x < -0.75f )
				{
					triangle.position.x = -0.75f;
					triangle.velocity.x = std::abs( triangle.velocity.x );
				}
				else if( triangle.position.x > 0.75f )
				{
					triangle.position.x = 0.75f;
					triangle.velocity.x = -std::abs( triangle.velocity.x );
				}
				if( triangle.position.y < -0.75f )
				{
					triangle.position.y = -0.75f;
					triangle.velocity.y = std::abs( triangle.velocity.y );
				}
				else if( triangle.position.y > 0.75f )
				{
					triangle.position.y = 0.75f;
					triangle.velocity.y = -std::abs( triangle.velocity.y );
				}
			}
		};

		std::chrono::steady_clock::time_point previousFrame = std::chrono::steady_clock::now();

		while( app.PumpMessages() )
		{
			if( app.IsWindowMinimized() )
			{
				WaitMessage();
				continue;
			}

			const std::chrono::steady_clock::time_point currentFrame = std::chrono::steady_clock::now();
			float deltaTime = std::chrono::duration<float>( currentFrame - previousFrame ).count();
			if( deltaTime > 0.05f )
			{
				deltaTime = 0.05f;
			}
			previousFrame = currentFrame;
			updateTriangles( triangles, deltaTime );

			const TextureHandle backbuffer = device.GetCurrentSwapchainTexture();
			RenderPass renderPass{};
			renderPass.color[ 0 ].loadOp = LoadOp::Clear;
			renderPass.color[ 0 ].clearColor = { 0.04f, 0.05f, 0.08f, 1.0f };

			Framebuffer framebuffer{};
			framebuffer.color[ 0 ].texture = backbuffer;

			ICommandBuffer& commands = device.AcquireCommandBuffer();
			commands.CmdBeginRendering( renderPass, framebuffer );
			commands.CmdBindRenderPipeline( gfx.pipeline );
			for( const MovingTriangle& triangle : triangles )
			{
				const PushConstants constants = { device.GetBindlessIndex( gfx.textureArray ),
					ToSamplerIndex( SamplerSlot::PointClamp ),
					triangle.layer,
					0.25f,
					triangle.position };
				commands.CmdPushConstants( &constants, sizeof( constants ) );
				commands.CmdDraw( 3 );
			}
			commands.CmdEndRendering();
			device.Submit( commands, backbuffer );
		}

		device.WaitIdle();
		device.Destroy( gfx.textureArray );
		gfx.pipeline = {};

		DeviceManager::ShutdownSingleton();
		gfx.deviceManager = nullptr;
		return 0;
	}
	catch( const std::exception& error )
	{
		DeviceManager::ShutdownSingleton();
		MessageBoxA( nullptr, error.what(), "Ldx12 Texture2DArray", MB_ICONERROR | MB_OK );
		return 1;
	}
}
