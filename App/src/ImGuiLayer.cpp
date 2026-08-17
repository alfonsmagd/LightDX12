#include "App/ImGuiLayer.hpp"

#include "imgui.h"
#include "backends/imgui_impl_dx12.h"
#include "backends/imgui_impl_win32.h"

#include <stdexcept>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler( HWND window, UINT message, WPARAM wParam, LPARAM lParam );

namespace App
{
	ImGuiLayer::ImGuiLayer( HWND window, ID3D12Device* device, ID3D12CommandQueue* commandQueue, DXGI_FORMAT renderTargetFormat, DXGI_FORMAT depthFormat, uint32_t framesInFlight )
	{
		if( !window || !device || !commandQueue ) throw std::invalid_argument( "ImGuiLayer received an invalid native handle." );
		device_ = device;
		D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
		heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		heapDesc.NumDescriptors = capacity_;
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		if( FAILED( device->CreateDescriptorHeap( &heapDesc, IID_PPV_ARGS( &descriptorHeap_ ) ) ) )
			throw std::runtime_error( "Cannot create the ImGui descriptor heap." );
		descriptorSize_ = device->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		ImGui::StyleColorsDark();
		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowRounding = 7.0f;
		style.FrameRounding = 4.0f;

		if( !ImGui_ImplWin32_Init( window ) ) throw std::runtime_error( "ImGui Win32 initialization failed." );
		ImGui_ImplDX12_InitInfo info;
		info.Device = device;
		info.CommandQueue = commandQueue;
		info.NumFramesInFlight = static_cast<int>( framesInFlight );
		info.RTVFormat = renderTargetFormat;
		info.DSVFormat = depthFormat;
		info.SrvDescriptorHeap = descriptorHeap_.Get();
		info.UserData = this;
		info.SrvDescriptorAllocFn = &AllocateDescriptor;
		info.SrvDescriptorFreeFn = &FreeDescriptor;
		if( !ImGui_ImplDX12_Init( &info ) ) throw std::runtime_error( "ImGui DirectX 12 initialization failed." );
		initialized_ = true;
	}

	ImGuiLayer::~ImGuiLayer()
	{
		if( initialized_ )
		{
			ImGui_ImplDX12_Shutdown();
			ImGui_ImplWin32_Shutdown();
			ImGui::DestroyContext();
		}
	}

	void ImGuiLayer::NewFrame()
	{
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
	}

	void ImGuiLayer::Render( ID3D12GraphicsCommandList* commandList )
	{
		ImGui::Render();
		ID3D12DescriptorHeap* heaps[] = { descriptorHeap_.Get() };
		commandList->SetDescriptorHeaps( 1, heaps );
		ImGui_ImplDX12_RenderDrawData( ImGui::GetDrawData(), commandList );
	}

	uint64_t ImGuiLayer::RegisterTexture( ID3D12Resource* resource, DXGI_FORMAT format )
	{
		if( !resource || format == DXGI_FORMAT_UNKNOWN ) throw std::invalid_argument( "RegisterTexture requires a texture and format." );
		const D3D12_RESOURCE_DESC resourceDesc = resource->GetDesc();
		if( resourceDesc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || resourceDesc.DepthOrArraySize != 1 )
			throw std::invalid_argument( "ImGuiLayer currently supports single-slice Texture2D previews." );

		const uint32_t index = AllocateDescriptorSlot();
		D3D12_CPU_DESCRIPTOR_HANDLE cpu{};
		D3D12_GPU_DESCRIPTOR_HANDLE gpu{};
		DescriptorHandles( index, cpu, gpu );
		D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
		srv.Format = format;
		srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		srv.Texture2D.MipLevels = resourceDesc.MipLevels;
		device_->CreateShaderResourceView( resource, &srv, cpu );
		return gpu.ptr;
	}

	void ImGuiLayer::UnregisterTexture( uint64_t textureId )
	{
		if( textureId == 0 || descriptorSize_ == 0 ) return;
		const UINT64 start = descriptorHeap_->GetGPUDescriptorHandleForHeapStart().ptr;
		if( textureId < start ) return;
		const UINT64 offset = textureId - start;
		if( offset % descriptorSize_ != 0 ) return;
		const uint32_t index = static_cast<uint32_t>( offset / descriptorSize_ );
		if( index < capacity_ ) allocatedMask_ &= ~( uint64_t{ 1 } << index );
	}

	bool ImGuiLayer::HandleMessage( HWND window, UINT message, WPARAM wParam, LPARAM lParam )
	{
		if( !ImGui::GetCurrentContext() ) return false;
		return ImGui_ImplWin32_WndProcHandler( window, message, wParam, lParam ) != 0;
	}

	uint32_t ImGuiLayer::AllocateDescriptorSlot()
	{
		for( uint32_t index = 0; index < capacity_; ++index )
		{
			const uint64_t bit = uint64_t{ 1 } << index;
			if( ( allocatedMask_ & bit ) != 0 ) continue;
			allocatedMask_ |= bit;
			return index;
		}
		throw std::runtime_error( "ImGui descriptor heap is full." );
	}

	void ImGuiLayer::DescriptorHandles( uint32_t index, D3D12_CPU_DESCRIPTOR_HANDLE& cpu, D3D12_GPU_DESCRIPTOR_HANDLE& gpu ) const
	{
		cpu = descriptorHeap_->GetCPUDescriptorHandleForHeapStart();
		gpu = descriptorHeap_->GetGPUDescriptorHandleForHeapStart();
		cpu.ptr += static_cast<SIZE_T>( index ) * descriptorSize_;
		gpu.ptr += static_cast<UINT64>( index ) * descriptorSize_;
	}

	void ImGuiLayer::AllocateDescriptor( ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* cpu, D3D12_GPU_DESCRIPTOR_HANDLE* gpu )
	{
		auto* layer = static_cast<ImGuiLayer*>( info->UserData );
		const uint32_t index = layer->AllocateDescriptorSlot();
		layer->DescriptorHandles( index, *cpu, *gpu );
	}

	void ImGuiLayer::FreeDescriptor( ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE )
	{
		auto* layer = static_cast<ImGuiLayer*>( info->UserData );
		const SIZE_T start = layer->descriptorHeap_->GetCPUDescriptorHandleForHeapStart().ptr;
		if( cpu.ptr < start || layer->descriptorSize_ == 0 ) return;
		const uint32_t index = static_cast<uint32_t>( ( cpu.ptr - start ) / layer->descriptorSize_ );
		if( index < layer->capacity_ ) layer->allocatedMask_ &= ~( uint64_t{ 1 } << index );
	}
}
