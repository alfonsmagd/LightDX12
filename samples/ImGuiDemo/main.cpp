#include "App/imgui_impl_ldx12.h"
#include "Ldx12/Ldx12.hpp"
#include "Ldx12Utils/AppLdx.hpp"
#include "Ldx12Utils/TextureLoader.hpp"

#include "imgui.h"
#include "backends/imgui_impl_win32.h"

#include <array>
#include <cstdint>
#include <exception>
#include <stdexcept>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler( HWND window, UINT message, WPARAM wParam, LPARAM lParam );

using namespace ldx12;

namespace
{
	constexpr uint32_t ourFramesInFlight = 3;

	bool HandleImGuiMessage( HWND window, UINT message, WPARAM wParam, LPARAM lParam, void* )
	{
		return ImGui::GetCurrentContext() != nullptr && ImGui_ImplWin32_WndProcHandler( window, message, wParam, lParam ) != 0;
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
		appDesc.className = L"Ldx12ImGuiDemoWindow";
		appDesc.title = L"Ldx12 - Dear ImGui bindless integration";
		appDesc.width = initialWidth;
		appDesc.height = initialHeight;
		appDesc.messageHandler = HandleImGuiMessage;
		utils::AppLdx app( appDesc );

		// 2. Initialize Ldx12.
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

		// 3. Create a sampled Ldx12 texture. Ldx12 creates its bindless SRV.
		const TextureHandle checkerTexture = utils::CreateCheckerTexture( device );

		// 4. Keep ImGui's Win32 backend and render with imgui_impl_ldx12.
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

		std::array<SubmitHandle, ourFramesInFlight> frameSubmissions{};
		uint32_t frameIndex = 0;
		int buttonPressCount = 0;
		float demoValue = 0.5f;
		while( app.PumpMessages() )
		{
			if( app.IsWindowMinimized() )
			{
				WaitMessage();
				continue;
			}

			device.Wait( frameSubmissions[ frameIndex ] );

			// 5. TextureHandle can be passed directly to ImGui::Image().
			ImGui_ImplLdx12_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();
			ImGui::Begin( "Ldx12 + Dear ImGui" );
			ImGui::TextUnformatted( "Dear ImGui is rendering directly through Ldx12." );
			ImGui::TextUnformatted( "64x64 Ldx12 texture:" );
			ImGui::Image( checkerTexture, ImVec2( 128.0f, 128.0f ) );
			ImGui::SliderFloat( "Value", &demoValue, 0.0f, 1.0f );
			if( ImGui::Button( "Press me" ) )
			{
				buttonPressCount++;
			}
			ImGui::SameLine();
			ImGui::Text( "Pressed %d times", buttonPressCount );
			ImGui::End();

			// 6. Render ImGui through the same Ldx12 command buffer and bindless heap.
			CommandBuffer& commands = device.AcquireCommandBuffer();
			const TextureHandle backBuffer = device.GetCurrentSwapchainTexture();
			RenderPass renderPass{};
			renderPass.color[ 0 ].loadOp = LoadOp::Clear;
			renderPass.color[ 0 ].clearColor = { 0.04f, 0.05f, 0.08f, 1.0f };
			Framebuffer framebuffer{};
			framebuffer.color[ 0 ].texture = backBuffer;

			commands.CmdBeginRendering( renderPass, framebuffer );
			ImGui::Render();
			ImGui_ImplLdx12_RenderDrawData( ImGui::GetDrawData(), commands );
			commands.CmdEndRendering();

			frameSubmissions[ frameIndex ] = device.Submit( commands, backBuffer );
			frameIndex = ( frameIndex + 1u ) % ourFramesInFlight;
		}

		// 7. Shut down while Ldx12 is still alive.
		manager.WaitIdle();
		ImGui_ImplLdx12_Shutdown();
		imguiLdx12Initialized = false;
		ImGui_ImplWin32_Shutdown();
		imguiWin32Initialized = false;
		ImGui::DestroyContext();
		device.Destroy( checkerTexture );

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
		MessageBoxA( nullptr, error.what(), "Ldx12 ImGuiDemo", MB_ICONERROR | MB_OK );
		return 1;
	}
}
