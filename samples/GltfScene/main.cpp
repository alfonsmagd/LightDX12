#include "Ldx12/HLSLLoader.hpp"
#include "Ldx12Utils/AppLdx.hpp"
#include "Ldx12Utils/DepthTarget.hpp"
#include "Ldx12Utils/GltfLoader.hpp"
#include "Ldx12Utils/OrbitCamera.hpp"
#include "SceneModel.hpp"
#include "SceneEnvironment.hpp"
#include "SceneTargets.hpp"
#include "App/imgui_impl_ldx12.h"

#include "backends/imgui_impl_win32.h"
#include <commdlg.h>

#include <algorithm>
#include <chrono>
#include <exception>
#include <vector>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler( HWND window, UINT message, WPARAM wParam, LPARAM lParam );

using namespace DirectX;
using namespace ldx12;

namespace
{
	enum class PresentationMode : uint8_t
	{
		VSync,
		UncappedTearing,
	};

	struct Constants
	{
		XMFLOAT4X4 viewProjection;
		XMFLOAT4 baseColor;

		XMFLOAT3 emissive;
		float metallic;

		XMFLOAT3 cameraPosition;
		float roughness;

		float normalScale;
		float occlusionStrength;
		uint32_t baseColorTexture;
		uint32_t metallicRoughnessTexture;

		uint32_t normalTexture;
		uint32_t occlusionTexture;
		uint32_t emissiveTexture;
		uint32_t baseColorSampler;

		uint32_t metallicRoughnessSampler;
		uint32_t normalSampler;
		uint32_t occlusionSampler;
		uint32_t emissiveSampler;

		std::array<uint32_t, 12> prefilteredTextures;
		uint32_t irradianceTexture;
		uint32_t brdfTexture;
		uint32_t environmentSampler;
		uint32_t environmentLevels;

		float exposure;
		uint32_t debugView;
		float textureLodBias;
		float environmentIntensity;

		float alphaCutoff;
	};

	static_assert( sizeof( Constants ) == 244 );

	bool HandleImGuiMessage( HWND window, UINT message, WPARAM wParam, LPARAM lParam, void* )
	{
		return ImGui::GetCurrentContext() != nullptr && ImGui_ImplWin32_WndProcHandler( window, message, wParam, lParam ) != 0;
	}


	std::filesystem::path SelectSceneFile( HWND window )
	{
		std::array<wchar_t, 32768> filename{};
		const std::wstring directory = std::filesystem::path( LDX12_MEDIA_DIRECTORY ).wstring();

		OPENFILENAMEW dialog{};
		dialog.lStructSize = sizeof( dialog );
		dialog.hwndOwner = window;
		dialog.lpstrFilter = L"glTF scenes (*.gltf;*.glb)\0*.gltf;*.glb\0";
		dialog.lpstrFile = filename.data();
		dialog.nMaxFile = static_cast<DWORD>( filename.size() );
		dialog.lpstrInitialDir = directory.c_str();
		dialog.lpstrTitle = L"Load glTF scene";
		dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_EXPLORER;

		if( GetOpenFileNameW( &dialog ) )
		{
			return filename.data();
		}

		return {};
	}

	RenderPipelineState CreatePipeline( RenderDevice& device, DXGI_FORMAT format, uint32_t sampleCount )
	{
		RenderPipelineDesc desc{};
		desc.vertexShader = HLSLLoader::LoadStage( "shaders/GltfScene.hlsl", "vs_6_6", "VSMain" );
		desc.fragmentShader = HLSLLoader::LoadStage( "shaders/GltfScene.hlsl", "ps_6_6", "PSMain" );

		desc.color[ 0 ].format = format;
		desc.depthFormat = DXGI_FORMAT_D32_FLOAT;
		desc.sampleCount = sampleCount;

		desc.inputElements[ 0 ].semanticName = "POSITION";
		desc.inputElements[ 0 ].format = DXGI_FORMAT_R32G32B32_FLOAT;

		desc.inputElements[ 1 ].semanticName = "NORMAL";
		desc.inputElements[ 1 ].format = DXGI_FORMAT_R32G32B32_FLOAT;
		desc.inputElements[ 1 ].alignedByteOffset = sizeof( XMFLOAT3 );

		desc.inputElements[ 2 ].semanticName = "TEXCOORD";
		desc.inputElements[ 2 ].format = DXGI_FORMAT_R32G32_FLOAT;
		desc.inputElements[ 2 ].alignedByteOffset = sizeof( XMFLOAT3 ) * 2;

		desc.rasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		desc.depthStencilState.DepthEnable = TRUE;
		desc.depthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		desc.depthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		desc.depthStencilState.StencilEnable = FALSE;

		return device.CreateRenderPipeline( desc );
	}


	void SetMaterial( Constants& constants, const utils::GltfPrimitive& primitive,
		const gltf_sample::MaterialBindings& bindings )
	{
		constants.baseColor = primitive.baseColor;
		constants.emissive = primitive.emissive;
		constants.metallic = primitive.metallic;
		constants.roughness = primitive.roughness;
		constants.normalScale = primitive.normalScale;
		constants.occlusionStrength = primitive.occlusionStrength;
		constants.alphaCutoff = primitive.alphaCutoff;

		constants.baseColorTexture = bindings.baseColor.texture;
		constants.metallicRoughnessTexture = bindings.metallicRoughness.texture;
		constants.normalTexture = bindings.normal.texture;
		constants.occlusionTexture = bindings.occlusion.texture;
		constants.emissiveTexture = bindings.emissive.texture;

		constants.baseColorSampler = bindings.baseColor.sampler;
		constants.metallicRoughnessSampler = bindings.metallicRoughness.sampler;
		constants.normalSampler = bindings.normal.sampler;
		constants.occlusionSampler = bindings.occlusion.sampler;
		constants.emissiveSampler = bindings.emissive.sampler;
	}
}

int WINAPI wWinMain( HINSTANCE instance, HINSTANCE, PWSTR, int showCommand )
{
	bool imguiWin32Initialized = false;
	bool imguiLdx12Initialized = false;

	try
	{
		utils::AppLdxDesc appDesc{};
		appDesc.instance = instance;
		appDesc.showCommand = showCommand;
		appDesc.className = L"Ldx12GltfSceneWindow";
		appDesc.title = L"Ldx12 PBR - 1: Lit  2: Albedo  3: Normals  4: Roughness  5: Metallic  6: AO  7: Emissive  8: Mip  M: Toggle mips";
		appDesc.width = 1280;
		appDesc.height = 720;
		appDesc.messageHandler = HandleImGuiMessage;

		utils::AppLdx app( appDesc );

		ContextDesc context{};
		context.enableDebugLayer = true;
		context.allowTearing = true;

		SwapchainDesc swapchain{};
		swapchain.window = MakeWin32WindowHandle( app.GetWindow() );
		swapchain.width = app.GetWidth();
		swapchain.height = app.GetHeight();
		swapchain.vsync = true;

		DeviceManager& manager = DeviceManager::Initialize( context, swapchain );
		app.SetDeviceManager( manager );
		RenderDevice& device = *manager.GetRenderDevice();

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsDark();
		ImGui::GetIO().IniFilename = nullptr;
		imguiWin32Initialized = ImGui_ImplWin32_Init( app.GetWindow() );

		ImGui_ImplLdx12_InitInfo imguiInfo{};
		imguiInfo.device = &device;
		imguiInfo.renderTargetFormat = context.swapchainFormat;
		imguiLdx12Initialized = ImGui_ImplLdx12_Init( imguiInfo );

		if( !imguiWin32Initialized || !imguiLdx12Initialized )
		{
			throw std::runtime_error( "Cannot initialize ImGui." );
		}

		{
			HLSLLoader::SetRootDirectory( std::filesystem::path( __FILE__ ).parent_path() );
			RenderPipelineState pipeline = CreatePipeline( device, context.swapchainFormat, 1 );
			RenderPipelineState msaaPipeline = CreatePipeline( device, context.swapchainFormat, 4 );
			gltf_sample::SceneModel model = gltf_sample::LoadModel( device,
				std::filesystem::path( LDX12_MEDIA_DIRECTORY ) / "gltfMesh/DamagedHelmet.gltf" );
			const gltf_sample::SceneEnvironment environment = gltf_sample::LoadEnvironment( device, std::filesystem::path( LDX12_MEDIA_DIRECTORY ) / "pbr" );

			utils::OrbitCamera camera;
			camera.distance = 4.2f;

			utils::DepthTarget depth( device );
			gltf_sample::SceneTargets msaaTargets;

				uint32_t debugView = 0;
				bool useMipmaps = true;
				bool useMsaa = true;
				PresentationMode presentationMode = PresentationMode::VSync;
				std::filesystem::path pendingPath;
			std::string loadError;
			auto titleUpdate = std::chrono::steady_clock::now();
			std::array<SubmitHandle, 3> frameSubmissions{};
			uint32_t frameIndex = 0;

			while( app.PumpMessages() )
			{
				if( app.IsWindowMinimized() )
				{
					WaitMessage();
					continue;
				}

				device.Wait( frameSubmissions[ frameIndex ] );

				if( !pendingPath.empty() )
				{
					try
					{
						gltf_sample::ReplaceModel( device, model, pendingPath );
						camera = {};
						camera.distance = 4.2f;
						loadError.clear();
					}
					catch( const std::exception& error )
					{
						loadError = error.what();
					}

					pendingPath.clear();
				}

				ImGui_ImplLdx12_NewFrame();
				ImGui_ImplWin32_NewFrame();
				ImGui::NewFrame();
				ImGui::SetNextWindowPos( ImVec2( 12, 12 ), ImGuiCond_Once );
				ImGui::SetNextWindowSize( ImVec2( 320, 0 ), ImGuiCond_Once );
				ImGui::Begin( "Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize );

				if( ImGui::Button( "Load .gltf" ) )
				{
					pendingPath = SelectSceneFile( app.GetWindow() );
				}

				int presentationModeIndex = static_cast<int>( presentationMode );
				if( ImGui::Combo( "Presentation", &presentationModeIndex, "VSync\0Uncapped + tearing\0" ) )
				{
					presentationMode = static_cast<PresentationMode>( presentationModeIndex );
					manager.SetVsync( presentationMode == PresentationMode::VSync );
				}

				const std::u8string filename = model.path.filename().u8string();
				ImGui::TextUnformatted( reinterpret_cast<const char*>( filename.c_str() ) );
				ImGui::Text( "%.1f FPS | %.2f ms", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate );
				ImGui::Text( "%zu meshes | %zu textures", model.geometry.size(), model.materials.images.size() );

				if( !loadError.empty() )
				{
					ImGui::TextWrapped( "%s", loadError.c_str() );
				}

				ImGui::End();
				ImGui::Render();

				if( !ImGui::GetIO().WantCaptureMouse && !ImGui::GetIO().WantCaptureKeyboard )
				{
					camera.Update( app );

				for( uint32_t view = 0; view < 8; ++view )
				{
					if( app.WasKeyPressed( '1' + view ) )
					{
						debugView = view;
					}
				}

				if( app.WasKeyPressed( 'M' ) )
				{
					useMipmaps = !useMipmaps;
				}

				if( app.WasKeyPressed( 'A' ) )
				{
					useMsaa = !useMsaa;
				}

				if( app.WasKeyPressed( VK_UP ) )
				{
					camera.distance = std::max( 1.5f, camera.distance - 0.25f );
				}

				if( app.WasKeyPressed( VK_DOWN ) )
				{
					camera.distance += 0.25f;
				}
				}

				const uint32_t width = manager.GetWidth();
				const uint32_t height = manager.GetHeight();
				depth.Resize( width, height );
				gltf_sample::ResizeTargets( device, msaaTargets, width, height, context.swapchainFormat );

				const auto now = std::chrono::steady_clock::now();
				if( now - titleUpdate >= std::chrono::milliseconds( 500 ) )
				{
					const std::wstring title = L"Ldx12 PBR | " + model.path.filename().wstring() + L" | " +
						std::to_wstring( static_cast<int>( ImGui::GetIO().Framerate + 0.5f ) ) + L" FPS | " +
						std::to_wstring( width ) + L"x" + std::to_wstring( height ) +
						( presentationMode == PresentationMode::VSync ? L" | VSync" : L" | Uncapped + tearing" ) +
						( useMsaa ? L" | MSAA x4" : L" | MSAA off" ) + ( useMipmaps ? L" | Mips on" : L" | Mips off" ) +
						L" | A: MSAA  M: Mips  Up/Down: Zoom  1-8: Views";

					SetWindowTextW( app.GetWindow(), title.c_str() );
					titleUpdate = now;
				}

				const utils::GltfScene& scene = model.scene;
				const XMVECTOR center = ( XMLoadFloat3( &scene.boundsMin ) + XMLoadFloat3( &scene.boundsMax ) ) * 0.5f;
				const float extent = std::max( XMVectorGetX( XMVector3Length( XMLoadFloat3( &scene.boundsMax ) - XMLoadFloat3( &scene.boundsMin ) ) ), 0.01f );

				Constants constants{};
				const XMMATRIX fit = XMMatrixTranslationFromVector( -center ) * XMMatrixScaling( 4.0f / extent, 4.0f / extent, 4.0f / extent );
				const float aspect = static_cast<float>( width ) / height;
				const XMMATRIX projection = XMMatrixPerspectiveFovLH( XMConvertToRadians( 50.0f ), aspect, 0.01f, 100.0f );
				const XMMATRIX viewProjection = fit * camera.GetViewMatrix() * projection;

				XMStoreFloat3( &constants.cameraPosition, center + camera.GetPosition() * ( extent / 4.0f ) );
				XMStoreFloat4x4( &constants.viewProjection, XMMatrixTranspose( viewProjection ) );

				for( size_t level = 0; level < environment.specular.size(); ++level )
				{
					constants.prefilteredTextures[ level ] = device.GetBindlessIndex( environment.specular[ level ] );
				}

				constants.irradianceTexture = device.GetBindlessIndex( environment.irradiance );
				constants.brdfTexture = device.GetBindlessIndex( environment.brdf );
				constants.environmentSampler = ToSamplerIndex( SamplerSlot::LinearClamp );
				constants.environmentLevels = static_cast<uint32_t>( environment.specular.size() );
				constants.environmentIntensity = 1;
				constants.exposure = 1;
				constants.debugView = debugView;
				constants.textureLodBias = useMipmaps ? 0.0f : -16.0f;

				const TextureHandle backbuffer = device.GetCurrentSwapchainTexture();

				RenderPass pass{};
				pass.color[ 0 ].loadOp = LoadOp::Clear;
				pass.color[ 0 ].clearColor = { 1, 1, 1, 1 };
				pass.depthStencil.depthLoadOp = LoadOp::Clear;
				pass.depthStencil.clearDepth = 1;

				Framebuffer framebuffer{};
				framebuffer.color[ 0 ].texture = useMsaa ? msaaTargets.color : backbuffer;
				framebuffer.depthStencil.texture = useMsaa ? msaaTargets.depth : depth.GetTexture();

				CommandBuffer& commands = device.AcquireCommandBuffer();
				commands.CmdBeginRendering( pass, framebuffer );
				commands.CmdBindRenderPipeline( useMsaa ? msaaPipeline : pipeline );

				for( size_t index = 0; index < model.geometry.size(); ++index )
				{
					SetMaterial( constants, scene.primitives[ index ], model.materials.bindings[ index ] );
					commands.CmdPushConstants( &constants, sizeof( constants ) );
					commands.CmdBindVertexBuffer( model.geometry[ index ].vertexBuffer );
					commands.CmdBindIndexBuffer( model.geometry[ index ].indexBuffer );
					commands.CmdDrawIndexed( model.geometry[ index ].indexCount );
				}

				commands.CmdEndRendering();

				if( useMsaa )
				{
					commands.CmdResolveTexture( msaaTargets.color, backbuffer );
				}

				// Draw ImGui on the resolved backbuffer with its single-sample pipeline.
				RenderPass uiPass{};
				uiPass.color[ 0 ].loadOp = LoadOp::Load;
				Framebuffer uiFramebuffer{};
				uiFramebuffer.color[ 0 ].texture = backbuffer;

				commands.CmdBeginRendering( uiPass, uiFramebuffer );
				ImGui_ImplLdx12_RenderDrawData( ImGui::GetDrawData(), commands );
				commands.CmdEndRendering();

				frameSubmissions[ frameIndex ] = device.Submit( commands, backbuffer );
				frameIndex = ( frameIndex + 1 ) % static_cast<uint32_t>( frameSubmissions.size() );
			}

			device.WaitIdle();

			gltf_sample::DestroyModel( device, model );
			gltf_sample::DestroyTargets( device, msaaTargets );
			gltf_sample::DestroyEnvironment( device, environment );
		}

		ImGui_ImplLdx12_Shutdown();
		imguiLdx12Initialized = false;
		ImGui_ImplWin32_Shutdown();
		imguiWin32Initialized = false;
		ImGui::DestroyContext();

		DeviceManager::ShutdownSingleton();
		return 0;
	}
	catch( const std::exception& error )
	{
		if( imguiLdx12Initialized ) ImGui_ImplLdx12_Shutdown();
		if( imguiWin32Initialized ) ImGui_ImplWin32_Shutdown();
		if( ImGui::GetCurrentContext() != nullptr ) ImGui::DestroyContext();

		DeviceManager::ShutdownSingleton();

		MessageBoxA( nullptr, error.what(), "Ldx12 glTF scene failed", MB_ICONERROR | MB_OK );

		return 1;
	}
}
