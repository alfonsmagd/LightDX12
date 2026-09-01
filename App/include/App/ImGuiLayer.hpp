#pragma once

#include "Ldx12/Ldx12.hpp"

#include <windows.h>

namespace App
{
	class ImGuiLayer final
	{
	public:
		ImGuiLayer( HWND window, ldx12::RenderDevice& device, DXGI_FORMAT renderTargetFormat, DXGI_FORMAT depthFormat, uint32_t framesInFlight );
		~ImGuiLayer();

		ImGuiLayer( const ImGuiLayer& ) = delete;
		ImGuiLayer& operator=( const ImGuiLayer& ) = delete;

		void NewFrame();
		void Render( ldx12::CommandBuffer& commandBuffer );
		static bool HandleMessage( HWND window, UINT message, WPARAM wParam, LPARAM lParam );

	private:
		bool initialized_ = false;
	};
}
