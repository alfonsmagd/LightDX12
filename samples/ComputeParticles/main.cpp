#include "Ldx12/HLSLLoader.hpp"
#include "Ldx12/Ldx12.hpp"
#include "Ldx12Utils/AppLdx.hpp"
#include "Ldx12Utils/DepthTarget.hpp"

#include <DirectXMath.h>

#include <algorithm>
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
	constexpr uint32_t ourParticleCount = 1u << 23u;
	constexpr uint32_t ourComputeGroupSize = 256;
	constexpr float ourSphereRadius = 3.0f;
	constexpr float ourTau = XM_2PI;

	struct Particle
	{
		XMFLOAT3 position = {};
		float restRadius = ourSphereRadius;
		XMFLOAT3 velocity = {};
		float padding = 0.0f;
	};

	struct PushConstants
	{
		XMFLOAT4X4 viewProjection = {};
		uint32_t particleSrvIndex = 0;
		uint32_t particleUavIndex = 0;
		uint32_t particleCount = 0;
		float deltaTime = 0.0f;
		float impulse = 0.0f;
	};

	static_assert( sizeof( Particle ) == 32 );
	static_assert( sizeof( PushConstants ) == 84 );

	RenderPipelineState CreateRenderPipeline( RenderDevice& device )
	{
		RenderPipelineDesc desc{};
		desc.vertexShader = HLSLLoader::LoadStage( "shaders/ComputeParticles.hlsl", "vs_6_6", "VSMain" );
		desc.fragmentShader = HLSLLoader::LoadStage( "shaders/ComputeParticles.hlsl", "ps_6_6", "PSMain" );
		desc.colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.depthFormat = DXGI_FORMAT_D32_FLOAT;
		desc.primitiveType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
		desc.topology = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
		desc.rasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		desc.depthStencilState.DepthEnable = TRUE;
		desc.depthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		return device.CreateRenderPipeline( desc );
	}

	ComputePipelineState CreateComputePipeline( RenderDevice& device )
	{
		ComputePipelineDesc desc{};
		desc.computeShader = HLSLLoader::LoadStage( "shaders/ComputeParticles.hlsl", "cs_6_6", "CSMain" );
		return device.CreateComputePipeline( desc );
	}

	BufferHandle CreateParticleBuffer( RenderDevice& device )
	{
		std::vector<Particle> particles( ourParticleCount );
		std::mt19937 random( 42u );
		std::uniform_real_distribution<float> unitValue( 0.0f, 1.0f );
		std::uniform_real_distribution<float> height( -1.0f, 1.0f );
		std::uniform_real_distribution<float> radiusVariation( -0.08f, 0.08f );
		for( Particle& particle : particles )
		{
			const float angle = unitValue( random ) * ourTau;
			const float y = height( random );
			const float horizontalRadius = std::sqrt( 1.0f - y * y );
			particle.restRadius = ourSphereRadius + radiusVariation( random );
			particle.position = { std::cos( angle ) * horizontalRadius * particle.restRadius,
				y * particle.restRadius,
				std::sin( angle ) * horizontalRadius * particle.restRadius };
		}

		BufferDesc desc{};
		desc.debugName = "Compute particle buffer";
		desc.size = sizeof( Particle ) * particles.size();
		desc.stride = sizeof( Particle );
		desc.type = BufferType::Structured;
		desc.initialData = particles.data();
		desc.unorderedAccess = true;
		return device.CreateBuffer( desc );
	}

	PushConstants BuildPushConstants( RenderDevice& device, BufferHandle particles, float deltaTime, float impulse, float aspectRatio )
	{
		const XMMATRIX view = XMMatrixLookAtLH( XMVectorSet( 0.0f, 0.0f, -10.0f, 1.0f ), XMVectorZero(), XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f ) );
		const XMMATRIX projection = XMMatrixPerspectiveFovLH( XMConvertToRadians( 50.0f ), aspectRatio, 0.1f, 100.0f );

		PushConstants constants{};
		XMStoreFloat4x4( &constants.viewProjection, XMMatrixTranspose( view * projection ) );
		constants.particleSrvIndex = device.GetBindlessIndex( particles );
		constants.particleUavIndex = device.GetUnorderedAccessIndex( particles );
		constants.particleCount = ourParticleCount;
		constants.deltaTime = deltaTime;
		constants.impulse = impulse;
		return constants;
	}
}

int WINAPI wWinMain( HINSTANCE instance, HINSTANCE, PWSTR, int showCommand )
{
	try
	{
		constexpr uint32_t initialWidth = 1280;
		constexpr uint32_t initialHeight = 720;
		HLSLLoader::SetRootDirectory( std::filesystem::path( __FILE__ ).parent_path() );

		utils::AppLdxDesc appDesc{};
		appDesc.instance = instance;
		appDesc.showCommand = showCommand;
		appDesc.className = L"Ldx12ComputeParticlesWindow";
		appDesc.title = L"Ldx12 - GPU Particle Sphere - Click";
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

		RenderPipelineState renderPipeline = CreateRenderPipeline( device );
		ComputePipelineState computePipeline = CreateComputePipeline( device );
		BufferHandle particles = CreateParticleBuffer( device );
		utils::DepthTarget depthTarget( device );

		std::chrono::steady_clock::time_point previousFrame = std::chrono::steady_clock::now();
		bool wasMouseDown = false;
		while( app.PumpMessages() )
		{
			if( app.IsWindowMinimized() )
			{
				WaitMessage();
				continue;
			}

			const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
			const float deltaTime = std::min( std::chrono::duration<float>( now - previousFrame ).count(), 1.0f / 30.0f );
			previousFrame = now;

			const uint32_t width = manager.GetWidth();
			const uint32_t height = manager.GetHeight();
			depthTarget.Resize( width, height );
			const bool mouseDown = app.IsLeftMouseButtonDown();
			const float impulse = mouseDown && !wasMouseDown ? 4.0f : 0.0f;
			wasMouseDown = mouseDown;
			const float aspectRatio = static_cast<float>( width ) / static_cast<float>( height );
			const PushConstants constants = BuildPushConstants( device, particles, deltaTime, impulse, aspectRatio );

			CommandBuffer& commands = device.AcquireCommandBuffer();
			commands.CmdTransitionBuffer( particles, D3D12_RESOURCE_STATE_UNORDERED_ACCESS );
			commands.CmdBindComputePipeline( computePipeline );
			commands.CmdPushConstants( &constants, sizeof( constants ) );
			commands.CmdDispatch( ourParticleCount / ourComputeGroupSize );
			commands.CmdTransitionBuffer( particles, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE );

			const TextureHandle backbuffer = device.GetCurrentSwapchainTexture();

			RenderPass renderPass{};
			renderPass.color[ 0 ].loadOp = LoadOp::Clear;
			renderPass.color[ 0 ].clearColor = { 0.002f, 0.004f, 0.012f, 1.0f };
			renderPass.depthStencil.depthLoadOp = LoadOp::Clear;
			renderPass.depthStencil.clearDepth = 1.0f;
			Framebuffer framebuffer{};
			framebuffer.color[ 0 ].texture = backbuffer;
			framebuffer.depthStencil.texture = depthTarget.GetTexture();

			commands.CmdBeginRendering( renderPass, framebuffer );
			commands.CmdBindRenderPipeline( renderPipeline );
			commands.CmdPushConstants( &constants, sizeof( constants ) );
			commands.CmdDraw( ourParticleCount );
			commands.CmdEndRendering();

			device.Submit( commands, backbuffer );
		}

		device.WaitIdle();
		depthTarget.Reset();
		device.Destroy( particles );
		renderPipeline = {};
		computePipeline = {};
		DeviceManager::ShutdownSingleton();
		return 0;
	}
	catch( const std::exception& error )
	{
		DeviceManager::ShutdownSingleton();
		MessageBoxA( nullptr, error.what(), "Ldx12 Compute Particles failed", MB_ICONERROR | MB_OK );
		return 1;
	}
}
