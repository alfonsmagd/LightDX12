#pragma once

#include "Ldx12/Ldx12.hpp"

#include "imgui.h"

#include <cstdint>

struct ImGui_ImplLdx12_InitInfo
{
	ldx12::RenderDevice* device = nullptr;
	DXGI_FORMAT renderTargetFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT depthFormat = DXGI_FORMAT_UNKNOWN;
	uint32_t framesInFlight = 3;
};

IMGUI_IMPL_API bool ImGui_ImplLdx12_Init( const ImGui_ImplLdx12_InitInfo& info );
IMGUI_IMPL_API void ImGui_ImplLdx12_Shutdown();
IMGUI_IMPL_API void ImGui_ImplLdx12_NewFrame();
IMGUI_IMPL_API void ImGui_ImplLdx12_RenderDrawData( ImDrawData* drawData, ldx12::ICommandBuffer& commandBuffer );
IMGUI_IMPL_API ImTextureRef ImGui_ImplLdx12_Texture( ldx12::TextureHandle texture );

namespace ImGui
{
	IMGUI_IMPL_API void Image(
		ldx12::TextureHandle texture,
		const ImVec2& imageSize,
		const ImVec2& uv0 = ImVec2( 0.0f, 0.0f ),
		const ImVec2& uv1 = ImVec2( 1.0f, 1.0f ) );
}
