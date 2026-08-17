#pragma once

#include "LightD3D12/LightD3D12.hpp"

#include <windows.h>

#include <array>
#include <cstdint>

namespace App
{
	struct WindowApplicationDesc
	{
		const wchar_t* className = L"LightDX12WindowApplication";
		const wchar_t* title = L"LightDX12";
		const char* debugLabel = "LightDX12 Window Application";
		uint32_t width = 1280;
		uint32_t height = 720;
		std::array<float, 4> clearColor{ 0.01f, 0.015f, 0.018f, 1.0f };
		bool enableDebugLayer = true;
		bool vsync = true;
	};

	class IWindowApplication
	{
	public:
		virtual ~IWindowApplication() = default;

		virtual void Initialize( lightd3d12::RenderDevice& device, DXGI_FORMAT colorFormat ) = 0;
		virtual void Physics( float deltaSeconds ) = 0;
		virtual void Render( lightd3d12::ICommandBuffer& commands ) = 0;
		virtual void Shutdown() = 0;
		virtual bool ShouldClose() const noexcept { return false; }
	};

	bool IsKeyDown( int key ) noexcept;
	int RunWindowApplication( HINSTANCE instance, int showCommand, const WindowApplicationDesc& desc,
		IWindowApplication& application );
}
