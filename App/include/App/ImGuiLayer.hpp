#pragma once

#include <cstdint>
#include <memory>

#include <windows.h>
#include <d3d12.h>
#include <wrl/client.h>

struct ImGui_ImplDX12_InitInfo;

namespace App
{
	class ImGuiLayer final
	{
	public:
		ImGuiLayer( HWND window, ID3D12Device* device, ID3D12CommandQueue* commandQueue, DXGI_FORMAT renderTargetFormat, DXGI_FORMAT depthFormat, uint32_t framesInFlight );
		~ImGuiLayer();

		ImGuiLayer( const ImGuiLayer& ) = delete;
		ImGuiLayer& operator=( const ImGuiLayer& ) = delete;

		void NewFrame();
		void Render( ID3D12GraphicsCommandList* commandList );
		uint64_t RegisterTexture( ID3D12Resource* resource, DXGI_FORMAT format );
		void UnregisterTexture( uint64_t textureId );
		static bool HandleMessage( HWND window, UINT message, WPARAM wParam, LPARAM lParam );

	private:
		uint32_t AllocateDescriptorSlot();
		void DescriptorHandles( uint32_t index, D3D12_CPU_DESCRIPTOR_HANDLE& cpu, D3D12_GPU_DESCRIPTOR_HANDLE& gpu ) const;
		static void AllocateDescriptor( ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* cpu, D3D12_GPU_DESCRIPTOR_HANDLE* gpu );
		static void FreeDescriptor( ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE gpu );

		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap_;
		ID3D12Device* device_ = nullptr;
		uint32_t descriptorSize_ = 0;
		uint32_t capacity_ = 64;
		uint64_t allocatedMask_ = 0;
		bool initialized_ = false;
	};
}
