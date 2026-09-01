#include "App/imgui_impl_ldx12.h"
#include "Ldx12/HLSLLoader.hpp"
#include "Ldx12/Ldx12.hpp"
#include "Ldx12Utils/AppLdx.hpp"
#include "Ldx12Utils/DepthTarget.hpp"
#include "Ldx12Utils/Geometry.hpp"
#include "Ldx12Utils/OrbitCamera.hpp"

#include "backends/imgui_impl_win32.h"
#include "imgui.h"

#include <DirectXMath.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <random>
#include <string>
#include <vector>

using namespace DirectX;
using namespace ldx12;

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler( HWND window, UINT message, WPARAM wParam, LPARAM lParam );

namespace
{
	constexpr uint32_t ourWindTextureSize = 512;
	constexpr uint32_t ourThreadGroupSize = 8;
	constexpr uint32_t ourFramesInFlight = 3;
	constexpr float ourFieldWorldSize = 12.0f;

	enum class ObstacleShape
	{
		Cube,
		Sphere,
	};

	enum class WindFieldTest : uint32_t
	{
		PositiveX,
		NegativeX,
		PositiveZ,
		NegativeZ,
		Vortices,
		Count,
	};

	struct Obstacle
	{
		ObstacleShape shape = ObstacleShape::Cube;
		XMFLOAT3 position = {};
		XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f };
		XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
		float rotationY = 0.0f;
	};

	struct WindTexel
	{
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
		float w = 0.0f;
	};

	struct Vortex
	{
		float centerX = 0.0f;
		float centerY = 0.0f;
		float radius = 0.0f;
		float strength = 0.0f;
	};

	constexpr std::array<Vortex, 3> ourVortices = { Vortex{ 0.28f, 0.28f, 0.22f, 1.35f },
		Vortex{ 0.67f, 0.48f, 0.25f, -1.45f },
		Vortex{ 0.38f, 0.76f, 0.20f, 1.20f } };

	struct PushConstants
	{
		XMFLOAT4X4 modelViewProjection = {};
		XMFLOAT4X4 model = {};
		XMFLOAT4 color = {};
		uint32_t inputWindIndex = 0;
		uint32_t obstacleMaskIndex = 0;
		uint32_t rawWakeIndex = 0;
		uint32_t rawWakeUavIndex = 0;
		uint32_t filteredWakeIndex = 0;
		uint32_t filteredWakeUavIndex = 0;
		uint32_t textureWidth = 0;
		uint32_t textureHeight = 0;
		uint32_t maxWakeSteps = 0;
		float fieldWorldSize = 0.0f;
		float edgeStep = 0.0f;
		float propagationStep = 0.0f;
		float wakeLength = 0.0f;
		uint32_t visualizeInputWind = 0;
		float wakeEffect = 1.0f;
	};

	static_assert( sizeof( PushConstants ) == 204 );

	bool HandleImGuiMessage( HWND window, UINT message, WPARAM wParam, LPARAM lParam, void* )
	{
		return ImGui::GetCurrentContext() != nullptr && ImGui_ImplWin32_WndProcHandler( window, message, wParam, lParam ) != 0;
	}

	RenderPipelineState CreateMaskPipeline( RenderDevice& device )
	{
		RenderPipelineDesc desc{};
		desc.vertexShader = HLSLLoader::LoadStage( "shaders/ObstacleMask.hlsl", "vs_6_6", "VSMain" );
		desc.fragmentShader = HLSLLoader::LoadStage( "shaders/ObstacleMask.hlsl", "ps_6_6", "PSMain" );
		desc.color[ 0 ].format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.colorFormat = DXGI_FORMAT_UNKNOWN;
		desc.inputElements[ 0 ].semanticName = "POSITION";
		desc.inputElements[ 0 ].format = DXGI_FORMAT_R32G32B32_FLOAT;
		desc.inputElements[ 0 ].alignedByteOffset = 0;
		desc.rasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		desc.depthStencilState.DepthEnable = FALSE;
		desc.depthStencilState.StencilEnable = FALSE;
		return device.CreateRenderPipeline( desc );
	}

	ComputePipelineState CreateWindPipeline( RenderDevice& device )
	{
		ComputePipelineDesc desc{};
		desc.computeShader = HLSLLoader::LoadStage( "shaders/WakeGenerate.hlsl", "cs_6_6", "CSMain" );
		return device.CreateComputePipeline( desc );
	}

	ComputePipelineState CreateWakeAveragePipeline( RenderDevice& device )
	{
		ComputePipelineDesc desc{};
		desc.computeShader = HLSLLoader::LoadStage( "shaders/WakeAverage.hlsl", "cs_6_6", "CSMain" );
		return device.CreateComputePipeline( desc );
	}

	RenderPipelineState CreateWakeClearPipeline( RenderDevice& device )
	{
		RenderPipelineDesc desc{};
		desc.vertexShader = HLSLLoader::LoadStage( "shaders/WakeClear.hlsl", "vs_6_6", "VSMain" );
		desc.fragmentShader = HLSLLoader::LoadStage( "shaders/WakeClear.hlsl", "ps_6_6", "PSMain" );
		desc.color[ 0 ].format = DXGI_FORMAT_R32_UINT;
		desc.colorFormat = DXGI_FORMAT_UNKNOWN;
		desc.rasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		desc.depthStencilState.DepthEnable = FALSE;
		desc.depthStencilState.StencilEnable = FALSE;
		return device.CreateRenderPipeline( desc );
	}

	RenderPipelineState CreateGroundPipeline( RenderDevice& device, DXGI_FORMAT swapchainFormat )
	{
		RenderPipelineDesc desc{};
		desc.vertexShader = HLSLLoader::LoadStage( "shaders/WindField.hlsl", "vs_6_6", "VSMain" );
		desc.fragmentShader = HLSLLoader::LoadStage( "shaders/WindField.hlsl", "ps_6_6", "PSMain" );
		desc.color[ 0 ].format = swapchainFormat;
		desc.colorFormat = DXGI_FORMAT_UNKNOWN;
		desc.depthFormat = DXGI_FORMAT_D32_FLOAT;
		desc.rasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		desc.depthStencilState.DepthEnable = TRUE;
		desc.depthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		desc.depthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		desc.depthStencilState.StencilEnable = FALSE;
		return device.CreateRenderPipeline( desc );
	}

	RenderPipelineState CreateWindPreviewPipeline( RenderDevice& device )
	{
		RenderPipelineDesc desc{};
		desc.vertexShader = HLSLLoader::LoadStage( "shaders/WindField.hlsl", "vs_6_6", "VSMain" );
		desc.fragmentShader = HLSLLoader::LoadStage( "shaders/WindField.hlsl", "ps_6_6", "PSMain" );
		desc.color[ 0 ].format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.colorFormat = DXGI_FORMAT_UNKNOWN;
		desc.rasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		desc.depthStencilState.DepthEnable = FALSE;
		desc.depthStencilState.StencilEnable = FALSE;
		return device.CreateRenderPipeline( desc );
	}

	RenderPipelineState CreateWakePreviewPipeline( RenderDevice& device )
	{
		RenderPipelineDesc desc{};
		desc.vertexShader = HLSLLoader::LoadStage( "shaders/WakePreview.hlsl", "vs_6_6", "VSMain" );
		desc.fragmentShader = HLSLLoader::LoadStage( "shaders/WakePreview.hlsl", "ps_6_6", "PSMain" );
		desc.color[ 0 ].format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.colorFormat = DXGI_FORMAT_UNKNOWN;
		desc.rasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		desc.depthStencilState.DepthEnable = FALSE;
		desc.depthStencilState.StencilEnable = FALSE;
		return device.CreateRenderPipeline( desc );
	}

	RenderPipelineState CreateSceneObstaclePipeline( RenderDevice& device, DXGI_FORMAT swapchainFormat )
	{
		RenderPipelineDesc desc{};
		desc.vertexShader = HLSLLoader::LoadStage( "shaders/SceneObstacle.hlsl", "vs_6_6", "VSMain" );
		desc.fragmentShader = HLSLLoader::LoadStage( "shaders/SceneObstacle.hlsl", "ps_6_6", "PSMain" );
		desc.color[ 0 ].format = swapchainFormat;
		desc.colorFormat = DXGI_FORMAT_UNKNOWN;
		desc.depthFormat = DXGI_FORMAT_D32_FLOAT;
		desc.inputElements[ 0 ].semanticName = "POSITION";
		desc.inputElements[ 0 ].format = DXGI_FORMAT_R32G32B32_FLOAT;
		desc.inputElements[ 0 ].alignedByteOffset = 0;
		desc.inputElements[ 1 ].semanticName = "NORMAL";
		desc.inputElements[ 1 ].format = DXGI_FORMAT_R32G32B32_FLOAT;
		desc.inputElements[ 1 ].alignedByteOffset = sizeof( XMFLOAT3 );
		desc.rasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		desc.depthStencilState.DepthEnable = TRUE;
		desc.depthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		desc.depthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		desc.depthStencilState.StencilEnable = FALSE;
		return device.CreateRenderPipeline( desc );
	}

	TextureHandle CreateInputWindTexture( RenderDevice& device, WindFieldTest windFieldTest )
	{
		const uint32_t texelCount = ourWindTextureSize * ourWindTextureSize;
		std::vector<WindTexel> wind( texelCount );
		for( uint32_t y = 0; y < ourWindTextureSize; ++y )
		{
			for( uint32_t x = 0; x < ourWindTextureSize; ++x )
			{
				const float textureX = ( static_cast<float>( x ) + 0.5f ) / static_cast<float>( ourWindTextureSize );
				const float textureY = ( static_cast<float>( y ) + 0.5f ) / static_cast<float>( ourWindTextureSize );
				float windX = 0.0f;
				float windY = 0.0f;
				switch( windFieldTest )
				{
				case WindFieldTest::PositiveX:
					windX = 1.0f;
					break;
				case WindFieldTest::NegativeX:
					windX = -1.0f;
					break;
				case WindFieldTest::PositiveZ:
					windY = -1.0f;
					break;
				case WindFieldTest::NegativeZ:
					windY = 1.0f;
					break;
				case WindFieldTest::Vortices:
					windX = 1.0f;
					break;
				case WindFieldTest::Count:
					break;
				}

				if( windFieldTest == WindFieldTest::Vortices )
				{
					for( const Vortex& vortex : ourVortices )
					{
						const float deltaX = textureX - vortex.centerX;
						const float deltaY = textureY - vortex.centerY;
						const float distanceSquared = deltaX * deltaX + deltaY * deltaY;
						const float distance = std::sqrt( distanceSquared + 0.0001f );
						const float falloff = std::exp( -distanceSquared / ( 2.0f * vortex.radius * vortex.radius ) );
						windX += ( -deltaY / distance ) * vortex.strength * falloff;
						windY += ( deltaX / distance ) * vortex.strength * falloff;
					}
				}

				const float windLength = std::sqrt( windX * windX + windY * windY + 0.000001f );
				wind[ y * ourWindTextureSize + x ] = { windX / windLength, windY / windLength, 0.0f, 1.0f };
			}
		}

		TextureDesc desc{};
		desc.debugName = "Input wind vector field";
		desc.width = ourWindTextureSize;
		desc.height = ourWindTextureSize;
		desc.format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		desc.usage = TextureUsage::Sampled;
		desc.data = wind.data();
		desc.rowPitch = ourWindTextureSize * sizeof( WindTexel );
		desc.slicePitch = desc.rowPitch * ourWindTextureSize;
		return device.CreateTexture( desc );
	}

	TextureHandle CreateObstacleMask( RenderDevice& device )
	{
		TextureDesc desc{};
		desc.debugName = "Wind obstacle mask";
		desc.width = ourWindTextureSize;
		desc.height = ourWindTextureSize;
		desc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.usage = TextureUsage::RenderTarget | TextureUsage::Sampled;
		desc.useClearValue = true;
		desc.clearValue.Format = desc.format;
		desc.clearValue.Color[ 0 ] = 1.0f;
		desc.clearValue.Color[ 1 ] = 1.0f;
		desc.clearValue.Color[ 2 ] = 1.0f;
		desc.clearValue.Color[ 3 ] = 1.0f;
		return device.CreateTexture( desc );
	}

	TextureHandle CreateWakeTexture( RenderDevice& device, const char* debugName, bool renderTarget )
	{
		TextureDesc desc{};
		desc.debugName = debugName;
		desc.width = ourWindTextureSize;
		desc.height = ourWindTextureSize;
		desc.format = DXGI_FORMAT_R32_UINT;
		desc.usage = TextureUsage::Sampled | TextureUsage::UnorderedAccess;
		if( renderTarget )
		{
			desc.usage |= TextureUsage::RenderTarget;
		}
		return device.CreateTexture( desc );
	}

	TextureHandle CreateWindPreviewTexture( RenderDevice& device, const char* debugName )
	{
		TextureDesc desc{};
		desc.debugName = debugName;
		desc.width = ourWindTextureSize;
		desc.height = ourWindTextureSize;
		desc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.usage = TextureUsage::RenderTarget | TextureUsage::Sampled;
		return device.CreateTexture( desc );
	}

	Obstacle CreateRandomObstacle( std::mt19937& random )
	{
		std::uniform_real_distribution<float> position( -ourFieldWorldSize * 0.36f, ourFieldWorldSize * 0.36f );
		std::uniform_real_distribution<float> horizontalScale( 0.35f, 0.75f );
		std::uniform_real_distribution<float> verticalScale( 0.45f, 1.25f );
		std::uniform_real_distribution<float> color( 0.25f, 1.0f );
		std::uniform_int_distribution<uint32_t> shape( 0, 1 );
		std::uniform_int_distribution<uint32_t> rotationDirection( 0, 1 );

		Obstacle obstacle{};
		obstacle.shape = shape( random ) == 0 ? ObstacleShape::Cube : ObstacleShape::Sphere;
		obstacle.position.x = position( random );
		obstacle.position.z = position( random );
		if( obstacle.shape == ObstacleShape::Sphere )
		{
			const float radius = horizontalScale( random );
			obstacle.scale = { radius, radius, radius };
			obstacle.position.y = radius;
		}
		else
		{
			obstacle.scale = { horizontalScale( random ), verticalScale( random ), horizontalScale( random ) };
			obstacle.position.y = obstacle.scale.y;
			obstacle.rotationY = rotationDirection( random ) == 0 ? XM_PIDIV4 : -XM_PIDIV4;
		}
		obstacle.color = { color( random ), color( random ), color( random ), 1.0f };
		return obstacle;
	}

	XMMATRIX GetObstacleModel( const Obstacle& obstacle )
	{
		return XMMatrixScaling( obstacle.scale.x, obstacle.scale.y, obstacle.scale.z ) * XMMatrixRotationY( obstacle.rotationY ) *
			XMMatrixTranslation( obstacle.position.x, obstacle.position.y, obstacle.position.z );
	}

	void SetTransforms( PushConstants& constants, const XMMATRIX& model, const XMMATRIX& viewProjection )
	{
		XMStoreFloat4x4( &constants.model, XMMatrixTranspose( model ) );
		XMStoreFloat4x4( &constants.modelViewProjection, XMMatrixTranspose( model * viewProjection ) );
	}

	void DrawObstacles( CommandBuffer& commands,
		const RenderPipelineState& pipeline,
		const utils::GeometryBuffers& cube,
		const utils::GeometryBuffers& sphere,
		const std::vector<Obstacle>& obstacles,
		const XMMATRIX& viewProjection,
		const PushConstants& sharedConstants )
	{
		commands.CmdBindRenderPipeline( pipeline );
		for( const Obstacle& obstacle : obstacles )
		{
			const utils::GeometryBuffers& geometry = obstacle.shape == ObstacleShape::Cube ? cube : sphere;
			PushConstants constants = sharedConstants;
			SetTransforms( constants, GetObstacleModel( obstacle ), viewProjection );
			constants.color = obstacle.color;
			commands.CmdBindVertexBuffer( geometry.vertexBuffer );
			commands.CmdBindIndexBuffer( geometry.indexBuffer );
			commands.CmdPushConstants( &constants, sizeof( constants ) );
			commands.CmdDrawIndexed( geometry.indexCount );
		}
	}
}

int WINAPI wWinMain( HINSTANCE instance, HINSTANCE, PWSTR, int showCommand )
{
	bool imguiWin32Initialized = false;
	bool imguiLdx12Initialized = false;

	try
	{
		constexpr uint32_t initialWidth = 1280;
		constexpr uint32_t initialHeight = 720;

		utils::AppLdxDesc appDesc{};
		appDesc.instance = instance;
		appDesc.showCommand = showCommand;
		appDesc.className = L"Ldx12WindOcclusionWindow";
		appDesc.title = L"Ldx12 - Compute wind occlusion - Space adds an obstacle";
		appDesc.width = initialWidth;
		appDesc.height = initialHeight;
		appDesc.messageHandler = HandleImGuiMessage;
		utils::AppLdx app( appDesc );

		HLSLLoader::SetRootDirectory( std::filesystem::path( __FILE__ ).parent_path() );

		ContextDesc contextDesc{};
		contextDesc.swapchainBufferCount = ourFramesInFlight;

		SwapchainDesc swapchainDesc{};
		swapchainDesc.window = MakeWin32WindowHandle( app.GetWindow() );
		swapchainDesc.width = initialWidth;
		swapchainDesc.height = initialHeight;
		swapchainDesc.vsync = true;

		DeviceManager& manager = DeviceManager::Initialize( contextDesc, swapchainDesc );
		app.SetDeviceManager( manager );
		RenderDevice& device = *manager.GetRenderDevice();

		RenderPipelineState maskPipeline = CreateMaskPipeline( device );
		ComputePipelineState windPipeline = CreateWindPipeline( device );
		ComputePipelineState wakeAveragePipeline = CreateWakeAveragePipeline( device );
		RenderPipelineState wakeClearPipeline = CreateWakeClearPipeline( device );
		RenderPipelineState groundPipeline = CreateGroundPipeline( device, contextDesc.swapchainFormat );
		RenderPipelineState windPreviewPipeline = CreateWindPreviewPipeline( device );
		RenderPipelineState wakePreviewPipeline = CreateWakePreviewPipeline( device );
		RenderPipelineState sceneObstaclePipeline = CreateSceneObstaclePipeline( device, contextDesc.swapchainFormat );
		std::array<TextureHandle, static_cast<uint32_t>( WindFieldTest::Count )> inputWindTextures{};
		inputWindTextures[ static_cast<uint32_t>( WindFieldTest::PositiveX ) ] = CreateInputWindTexture( device, WindFieldTest::PositiveX );
		inputWindTextures[ static_cast<uint32_t>( WindFieldTest::NegativeX ) ] = CreateInputWindTexture( device, WindFieldTest::NegativeX );
		inputWindTextures[ static_cast<uint32_t>( WindFieldTest::PositiveZ ) ] = CreateInputWindTexture( device, WindFieldTest::PositiveZ );
		inputWindTextures[ static_cast<uint32_t>( WindFieldTest::NegativeZ ) ] = CreateInputWindTexture( device, WindFieldTest::NegativeZ );
		inputWindTextures[ static_cast<uint32_t>( WindFieldTest::Vortices ) ] = CreateInputWindTexture( device, WindFieldTest::Vortices );
		TextureHandle obstacleMask = CreateObstacleMask( device );
		TextureHandle rawWakeTexture = CreateWakeTexture( device, "Raw wind wake factor", true );
		TextureHandle filteredWakeTexture = CreateWakeTexture( device, "Averaged wind wake factor", false );
		TextureHandle inputWindPreview = CreateWindPreviewTexture( device, "Input wind preview" );
		TextureHandle generatedWindPreview = CreateWindPreviewTexture( device, "Generated wind preview" );
		utils::GeometryBuffers cube = utils::CreateCube( device );
		utils::GeometryBuffers sphere = utils::CreateSphere( device, 12, 20 );

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsDark();
		if( !ImGui_ImplWin32_Init( app.GetWindow() ) )
		{
			throw std::runtime_error( "ImGui Win32 initialization failed." );
		}
		imguiWin32Initialized = true;

		ImGui_ImplLdx12_InitInfo imguiInfo{};
		imguiInfo.device = &device;
		imguiInfo.framesInFlight = ourFramesInFlight;
		imguiInfo.renderTargetFormat = contextDesc.swapchainFormat;
		if( !ImGui_ImplLdx12_Init( imguiInfo ) )
		{
			throw std::runtime_error( "ImGui Ldx12 initialization failed." );
		}
		imguiLdx12Initialized = true;

		PushConstants sharedConstants{};
		XMStoreFloat4x4( &sharedConstants.modelViewProjection, XMMatrixIdentity() );
		XMStoreFloat4x4( &sharedConstants.model, XMMatrixIdentity() );
		WindFieldTest activeWindTest = WindFieldTest::Vortices;
		sharedConstants.inputWindIndex = device.GetBindlessIndex(
			inputWindTextures[ static_cast<uint32_t>( activeWindTest ) ] );
		sharedConstants.obstacleMaskIndex = device.GetBindlessIndex( obstacleMask );
		sharedConstants.rawWakeIndex = device.GetBindlessIndex( rawWakeTexture );
		sharedConstants.rawWakeUavIndex = device.GetUnorderedAccessIndex( rawWakeTexture );
		sharedConstants.filteredWakeIndex = device.GetBindlessIndex( filteredWakeTexture );
		sharedConstants.filteredWakeUavIndex = device.GetUnorderedAccessIndex( filteredWakeTexture );
		sharedConstants.textureWidth = ourWindTextureSize;
		sharedConstants.textureHeight = ourWindTextureSize;
		sharedConstants.maxWakeSteps = 220;
		sharedConstants.fieldWorldSize = ourFieldWorldSize;
		sharedConstants.edgeStep = 1.25f;
		sharedConstants.propagationStep = 1.0f;
		sharedConstants.wakeLength = 20.0f;
		sharedConstants.wakeEffect = 1.0f;

		{
			std::mt19937 random( 0x4c445831u );
			std::vector<Obstacle> obstacles;
			obstacles.reserve( 64 );
			for( uint32_t index = 0; index < 8; ++index )
			{
				obstacles.push_back( CreateRandomObstacle( random ) );
			}

			utils::OrbitCamera camera;
			camera.yaw = 0.75f;
			camera.pitch = 0.60f;
			camera.distance = 15.0f;
			utils::DepthTarget depthTarget( device );
			std::array<SubmitHandle, ourFramesInFlight> frameSubmissions{};
			uint32_t frameIndex = 0;

			const XMMATRIX maskView = XMMatrixLookAtLH( XMVectorSet( 0.0f, 20.0f, 0.0f, 1.0f ),
				XMVectorZero(),
				XMVectorSet( 0.0f, 0.0f, 1.0f, 0.0f ) );
			const XMMATRIX maskProjection = XMMatrixOrthographicLH( ourFieldWorldSize, ourFieldWorldSize, 0.1f, 40.0f );
			const XMMATRIX maskViewProjection = maskView * maskProjection;

			while( app.PumpMessages() )
			{
				if( app.IsWindowMinimized() )
				{
					WaitMessage();
					continue;
				}

				device.Wait( frameSubmissions[ frameIndex ] );

				if( app.WasKeyPressed( VK_SPACE ) )
				{
					const Obstacle obstacle = CreateRandomObstacle( random );
					obstacles.push_back( obstacle );
					const std::wstring title = L"Ldx12 - Wind occlusion - " + std::to_wstring( obstacles.size() ) +
						( obstacle.shape == ObstacleShape::Cube ? L" obstacles - added cube" : L" obstacles - added sphere" );
					SetWindowTextW( app.GetWindow(), title.c_str() );
				}

				const uint32_t width = manager.GetWidth();
				const uint32_t height = manager.GetHeight();
				depthTarget.Resize( width, height );
				const float aspect = static_cast<float>( width ) / static_cast<float>( height );
				const XMMATRIX projection = XMMatrixPerspectiveFovLH( XMConvertToRadians( 58.0f ), aspect, 0.1f, 100.0f );
				ImGui_ImplLdx12_NewFrame();
				ImGui_ImplWin32_NewFrame();
				ImGui::NewFrame();
				ImGui::SetNextWindowPos( ImVec2( 10.0f, 10.0f ), ImGuiCond_Always );
				ImGui::SetNextWindowSize( ImVec2( 600.0f, 500.0f ), ImGuiCond_Always );
				ImGui::Begin( "Wind wake", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse );
				ImGui::Text( "%u x %u", ourWindTextureSize, ourWindTextureSize );
				ImGui::BeginGroup();
				ImGui::TextUnformatted( "Input: original wind" );
				ImGui::Image( inputWindPreview, ImVec2( 264.0f, 264.0f ) );
				ImGui::EndGroup();
				ImGui::SameLine();
				ImGui::BeginGroup();
				ImGui::TextUnformatted( "Wake: dark=low, white=full, red=solid" );
				ImGui::Image( generatedWindPreview, ImVec2( 264.0f, 264.0f ) );
				ImGui::EndGroup();
				ImGui::Separator();
				ImGui::TextUnformatted( "Test 1 - input wind" );
				if( ImGui::Button( "+X" ) ) activeWindTest = WindFieldTest::PositiveX;
				ImGui::SameLine();
				if( ImGui::Button( "-X" ) ) activeWindTest = WindFieldTest::NegativeX;
				ImGui::SameLine();
				if( ImGui::Button( "+Z" ) ) activeWindTest = WindFieldTest::PositiveZ;
				ImGui::SameLine();
				if( ImGui::Button( "-Z" ) ) activeWindTest = WindFieldTest::NegativeZ;
				ImGui::SameLine();
				if( ImGui::Button( "Vortices" ) ) activeWindTest = WindFieldTest::Vortices;
				ImGui::SliderFloat( "Edge step", &sharedConstants.edgeStep, 0.5f, 2.0f );
				ImGui::SliderFloat( "Propagation step", &sharedConstants.propagationStep, 0.5f, 2.0f );
				ImGui::SliderFloat( "Wake length", &sharedConstants.wakeLength, 1.0f, 256.0f );
				ImGui::SliderFloat( "Wake factor", &sharedConstants.wakeEffect, 0.0f, 1.0f );
				int maxWakeSteps = static_cast<int>( sharedConstants.maxWakeSteps );
				if( ImGui::SliderInt( "Max wake steps", &maxWakeSteps, 24, 512 ) )
				{
					sharedConstants.maxWakeSteps = static_cast<uint32_t>( maxWakeSteps );
				}
				const uint32_t requiredWakeSteps = static_cast<uint32_t>(
					std::ceil( sharedConstants.wakeLength / sharedConstants.propagationStep ) );
				if( sharedConstants.maxWakeSteps < requiredWakeSteps )
				{
					sharedConstants.maxWakeSteps = requiredWakeSteps;
				}
				ImGui::TextUnformatted( "Space: add a random cube or sphere" );
				ImGui::End();
				if( !ImGui::GetIO().WantCaptureMouse )
				{
					camera.Update( app );
				}
				ImGui::Render();
				const TextureHandle selectedInputWind = inputWindTextures[ static_cast<uint32_t>( activeWindTest ) ];
				sharedConstants.inputWindIndex = device.GetBindlessIndex(
					selectedInputWind );
				const XMMATRIX viewProjection = camera.GetViewMatrix() * projection;

				CommandBuffer& commands = device.AcquireCommandBuffer();

				// 1. Rasterize the dynamic solids into an SDF-like obstacle mask.
				RenderPass maskPass{};
				maskPass.color[ 0 ].loadOp = LoadOp::Clear;
				maskPass.color[ 0 ].clearColor = { 1.0f, 1.0f, 1.0f, 1.0f };
				Framebuffer maskFramebuffer{};
				maskFramebuffer.color[ 0 ].texture = obstacleMask;
				commands.CmdBeginRendering( maskPass, maskFramebuffer );
				DrawObstacles( commands, maskPipeline, cube, sphere, obstacles, maskViewProjection, sharedConstants );
				commands.CmdEndRendering();

				// 2. Clear the raw factor to full wind before the compute propagation.
				RenderPass wakeClearPass{};
				wakeClearPass.color[ 0 ].loadOp = LoadOp::DontCare;
				Framebuffer wakeClearFramebuffer{};
				wakeClearFramebuffer.color[ 0 ].texture = rawWakeTexture;
				commands.CmdBeginRendering( wakeClearPass, wakeClearFramebuffer );
				commands.CmdBindRenderPipeline( wakeClearPipeline );
				commands.CmdDraw( 3 );
				commands.CmdEndRendering();

				// 3. Only downwind edge seeds propagate through the original local wind field.
				commands.CmdTransitionTexture( selectedInputWind, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE );
				commands.CmdTransitionTexture( obstacleMask, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE );
				commands.CmdTransitionTexture( rawWakeTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS );
				commands.CmdBindComputePipeline( windPipeline );
				commands.CmdPushConstants( &sharedConstants, sizeof( sharedConstants ) );
				commands.CmdDispatch( ourWindTextureSize / ourThreadGroupSize, ourWindTextureSize / ourThreadGroupSize );

				// 4. Average the calculated occlusion in an 8 x 8 neighbourhood.
				commands.CmdTransitionTexture( rawWakeTexture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE );
				commands.CmdTransitionTexture( filteredWakeTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS );
				commands.CmdBindComputePipeline( wakeAveragePipeline );
				commands.CmdPushConstants( &sharedConstants, sizeof( sharedConstants ) );
				commands.CmdDispatch( ourWindTextureSize / ourThreadGroupSize, ourWindTextureSize / ourThreadGroupSize );
				commands.CmdTransitionTexture( selectedInputWind, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE );
				commands.CmdTransitionTexture( obstacleMask, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE );
				commands.CmdTransitionTexture( filteredWakeTexture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE );

				// 5. Render both states as readable 2D maps for the ImGui panel.
				PushConstants previewConstants = sharedConstants;
				SetTransforms( previewConstants, XMMatrixIdentity(), maskViewProjection );
				RenderPass previewPass{};
				previewPass.color[ 0 ].loadOp = LoadOp::DontCare;
				Framebuffer inputPreviewFramebuffer{};
				inputPreviewFramebuffer.color[ 0 ].texture = inputWindPreview;
				previewConstants.visualizeInputWind = 1;
				commands.CmdBeginRendering( previewPass, inputPreviewFramebuffer );
				commands.CmdBindRenderPipeline( windPreviewPipeline );
				commands.CmdPushConstants( &previewConstants, sizeof( previewConstants ) );
				commands.CmdDraw( 6 );
				commands.CmdEndRendering();

				Framebuffer generatedPreviewFramebuffer{};
				generatedPreviewFramebuffer.color[ 0 ].texture = generatedWindPreview;
				previewConstants.visualizeInputWind = 0;
				commands.CmdBeginRendering( previewPass, generatedPreviewFramebuffer );
				commands.CmdBindRenderPipeline( wakePreviewPipeline );
				commands.CmdPushConstants( &previewConstants, sizeof( previewConstants ) );
				commands.CmdDraw( 6 );
				commands.CmdEndRendering();
				commands.CmdTransitionTexture( inputWindPreview, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE );
				commands.CmdTransitionTexture( generatedWindPreview, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE );

				// 6. Draw the attenuated wind field on the ground and the same obstacles in 3D.
				const TextureHandle backbuffer = device.GetCurrentSwapchainTexture();
				RenderPass scenePass{};
				scenePass.color[ 0 ].loadOp = LoadOp::Clear;
				scenePass.color[ 0 ].clearColor = { 0.008f, 0.012f, 0.02f, 1.0f };
				scenePass.depthStencil.depthLoadOp = LoadOp::Clear;
				scenePass.depthStencil.clearDepth = 1.0f;
				Framebuffer sceneFramebuffer{};
				sceneFramebuffer.color[ 0 ].texture = backbuffer;
				sceneFramebuffer.depthStencil.texture = depthTarget.GetTexture();
				commands.CmdBeginRendering( scenePass, sceneFramebuffer );

				PushConstants groundConstants = sharedConstants;
				SetTransforms( groundConstants, XMMatrixIdentity(), viewProjection );
				commands.CmdBindRenderPipeline( groundPipeline );
				commands.CmdPushConstants( &groundConstants, sizeof( groundConstants ) );
				commands.CmdDraw( 6 );
				DrawObstacles( commands, sceneObstaclePipeline, cube, sphere, obstacles, viewProjection, sharedConstants );
				commands.CmdEndRendering();

				RenderPass uiPass{};
				uiPass.color[ 0 ].loadOp = LoadOp::Load;
				Framebuffer uiFramebuffer{};
				uiFramebuffer.color[ 0 ].texture = backbuffer;
				commands.CmdBeginRendering( uiPass, uiFramebuffer );
				ImGui_ImplLdx12_RenderDrawData( ImGui::GetDrawData(), commands );
				commands.CmdEndRendering();

				frameSubmissions[ frameIndex ] = device.Submit( commands, backbuffer );
				frameIndex = ( frameIndex + 1u ) % ourFramesInFlight;
			}

			device.WaitIdle();
		}

		ImGui_ImplLdx12_Shutdown();
		imguiLdx12Initialized = false;
		ImGui_ImplWin32_Shutdown();
		imguiWin32Initialized = false;
		ImGui::DestroyContext();

		utils::DestroyGeometry( device, sphere );
		utils::DestroyGeometry( device, cube );
		device.Destroy( generatedWindPreview );
		device.Destroy( inputWindPreview );
		device.Destroy( filteredWakeTexture );
		device.Destroy( rawWakeTexture );
		device.Destroy( obstacleMask );
		for( TextureHandle& inputWindTexture : inputWindTextures )
		{
			device.Destroy( inputWindTexture );
		}
		sceneObstaclePipeline = {};
		wakePreviewPipeline = {};
		windPreviewPipeline = {};
		groundPipeline = {};
		wakeClearPipeline = {};
		wakeAveragePipeline = {};
		windPipeline = {};
		maskPipeline = {};
		DeviceManager::ShutdownSingleton();
		return 0;
	}
	catch( const std::exception& error )
	{
		if( imguiLdx12Initialized )
		{
			ImGui_ImplLdx12_Shutdown();
		}
		if( imguiWin32Initialized )
		{
			ImGui_ImplWin32_Shutdown();
		}
		if( ImGui::GetCurrentContext() != nullptr )
		{
			ImGui::DestroyContext();
		}
		DeviceManager::ShutdownSingleton();
		MessageBoxA( nullptr, error.what(), "Ldx12 WindOcclusion failed", MB_ICONERROR | MB_OK );
		return 1;
	}
}
