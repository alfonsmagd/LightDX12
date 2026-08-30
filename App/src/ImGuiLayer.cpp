#include "App/ImGuiLayer.hpp"

#include "App/imgui_impl_ldx12.h"

#include "imgui.h"
#include "backends/imgui_impl_win32.h"

#include <stdexcept>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler( HWND window, UINT message, WPARAM wParam, LPARAM lParam );

namespace App
{
	ImGuiLayer::ImGuiLayer( HWND window, ldx12::RenderDevice& device, DXGI_FORMAT renderTargetFormat, DXGI_FORMAT depthFormat, uint32_t framesInFlight )
	{
		if( window == nullptr )
		{
			throw std::invalid_argument( "ImGuiLayer received an invalid window." );
		}

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		ImGui::StyleColorsDark();
		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowRounding = 7.0f;
		style.FrameRounding = 4.0f;

		if( !ImGui_ImplWin32_Init( window ) )
		{
			ImGui::DestroyContext();
			throw std::runtime_error( "ImGui Win32 initialization failed." );
		}

		ImGui_ImplLdx12_InitInfo info{};
		info.device = &device;
		info.renderTargetFormat = renderTargetFormat;
		info.depthFormat = depthFormat;
		info.framesInFlight = framesInFlight;
		if( !ImGui_ImplLdx12_Init( info ) )
		{
			ImGui_ImplWin32_Shutdown();
			ImGui::DestroyContext();
			throw std::runtime_error( "ImGui Ldx12 initialization failed." );
		}
		initialized_ = true;
	}

	ImGuiLayer::~ImGuiLayer()
	{
		if( initialized_ )
		{
			ImGui_ImplLdx12_Shutdown();
			ImGui_ImplWin32_Shutdown();
			ImGui::DestroyContext();
		}
	}

	void ImGuiLayer::NewFrame()
	{
		ImGui_ImplLdx12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
	}

	void ImGuiLayer::Render( ldx12::ICommandBuffer& commandBuffer )
	{
		ImGui::Render();
		ImGui_ImplLdx12_RenderDrawData( ImGui::GetDrawData(), commandBuffer );
	}

	bool ImGuiLayer::HandleMessage( HWND window, UINT message, WPARAM wParam, LPARAM lParam )
	{
		if( ImGui::GetCurrentContext() == nullptr )
		{
			return false;
		}
		return ImGui_ImplWin32_WndProcHandler( window, message, wParam, lParam ) != 0;
	}
}
