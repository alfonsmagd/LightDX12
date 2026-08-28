#include "imgui.h"
#include "backends/imgui_impl_dx12.h"
#include "backends/imgui_impl_win32.h"

#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <exception>
#include <stdexcept>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler( HWND window, UINT message, WPARAM wParam, LPARAM lParam );

using Microsoft::WRL::ComPtr;

namespace
{
	constexpr uint32_t ourFramesInFlight = 3;
	constexpr uint32_t ourImGuiDescriptorCapacity = 64;
	constexpr DXGI_FORMAT ourRenderTargetFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

	void CheckResult( HRESULT result, const char* message )
	{
		if( FAILED( result ) )
		{
			throw std::runtime_error( message );
		}
	}

	struct NativeDescriptorHeap final
	{
		ComPtr<ID3D12DescriptorHeap> heap{};
		std::array<bool, ourImGuiDescriptorCapacity> allocated{};
		uint32_t descriptorSize = 0;

		uint32_t Allocate()
		{
			for( uint32_t index = 0; index < allocated.size(); ++index )
			{
				if( !allocated[ index ] )
				{
					allocated[ index ] = true;
					return index;
				}
			}
			throw std::runtime_error( "The native ImGui descriptor heap is full." );
		}

		void Free( uint32_t index ) noexcept
		{
			if( index < allocated.size() )
			{
				allocated[ index ] = false;
			}
		}

		void GetHandles(
			uint32_t index,
			D3D12_CPU_DESCRIPTOR_HANDLE& cpu,
			D3D12_GPU_DESCRIPTOR_HANDLE& gpu ) const noexcept
		{
			cpu = heap->GetCPUDescriptorHandleForHeapStart();
			gpu = heap->GetGPUDescriptorHandleForHeapStart();
			cpu.ptr += static_cast<SIZE_T>( index ) * descriptorSize;
			gpu.ptr += static_cast<UINT64>( index ) * descriptorSize;
		}
	};

	void AllocateImGuiDescriptor(
		ImGui_ImplDX12_InitInfo* info,
		D3D12_CPU_DESCRIPTOR_HANDLE* cpu,
		D3D12_GPU_DESCRIPTOR_HANDLE* gpu )
	{
		NativeDescriptorHeap* descriptors = static_cast<NativeDescriptorHeap*>( info->UserData );
		const uint32_t index = descriptors->Allocate();
		descriptors->GetHandles( index, *cpu, *gpu );
	}

	void FreeImGuiDescriptor(
		ImGui_ImplDX12_InitInfo* info,
		D3D12_CPU_DESCRIPTOR_HANDLE cpu,
		D3D12_GPU_DESCRIPTOR_HANDLE )
	{
		NativeDescriptorHeap* descriptors = static_cast<NativeDescriptorHeap*>( info->UserData );
		const SIZE_T firstDescriptor = descriptors->heap->GetCPUDescriptorHandleForHeapStart().ptr;
		if( cpu.ptr < firstDescriptor || descriptors->descriptorSize == 0 )
		{
			return;
		}
		const SIZE_T offset = cpu.ptr - firstDescriptor;
		if( offset % descriptors->descriptorSize == 0 )
		{
			descriptors->Free( static_cast<uint32_t>( offset / descriptors->descriptorSize ) );
		}
	}

	struct NativeFrame final
	{
		ComPtr<ID3D12CommandAllocator> allocator{};
		uint64_t fenceValue = 0;
	};

	struct NativeTexture final
	{
		ComPtr<ID3D12Resource> resource{};
		uint32_t descriptorIndex = UINT32_MAX;
		ImTextureRef imguiTexture{};
	};

	struct NativeRenderer final
	{
		ComPtr<IDXGIFactory6> factory{};
		ComPtr<ID3D12Device> device{};
		ComPtr<ID3D12CommandQueue> queue{};
		ComPtr<IDXGISwapChain3> swapchain{};
		ComPtr<ID3D12DescriptorHeap> rtvHeap{};

		NativeDescriptorHeap imguiDescriptors{};

		std::array<ComPtr<ID3D12Resource>, ourFramesInFlight> backbuffers{};
		std::array<NativeFrame, ourFramesInFlight> frames{};

		ComPtr<ID3D12GraphicsCommandList> commandList{};
		ComPtr<ID3D12Fence> fence{};

		HANDLE fenceEvent = nullptr;
		uint64_t nextFenceValue = 0;
		uint32_t rtvDescriptorSize = 0;
		uint32_t currentBackbuffer = 0;

		void Initialize( HWND window, uint32_t width, uint32_t height )
		{
			CheckResult( CreateDXGIFactory2( 0, IID_PPV_ARGS( factory.GetAddressOf() ) ), "Failed to create the native DXGI factory." );
			CheckResult( D3D12CreateDevice( nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS( device.GetAddressOf() ) ), "Failed to create the native D3D12 device." );

			D3D12_COMMAND_QUEUE_DESC queueDesc{};
			queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
			CheckResult( device->CreateCommandQueue( &queueDesc, IID_PPV_ARGS( queue.GetAddressOf() ) ), "Failed to create the native command queue." );

			DXGI_SWAP_CHAIN_DESC1 swapchainDesc{};
			swapchainDesc.Width = width;
			swapchainDesc.Height = height;
			swapchainDesc.Format = ourRenderTargetFormat;
			swapchainDesc.SampleDesc.Count = 1;
			swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			swapchainDesc.BufferCount = ourFramesInFlight;
			swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

			ComPtr<IDXGISwapChain1> initialSwapchain{};
			CheckResult(
				factory->CreateSwapChainForHwnd( queue.Get(), window, &swapchainDesc, nullptr, nullptr, initialSwapchain.GetAddressOf() ),
				"Failed to create the native swapchain." );
			CheckResult( initialSwapchain.As( &swapchain ), "Failed to query IDXGISwapChain3." );
			factory->MakeWindowAssociation( window, DXGI_MWA_NO_ALT_ENTER );

			D3D12_DESCRIPTOR_HEAP_DESC rtvDesc{};
			rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
			rtvDesc.NumDescriptors = ourFramesInFlight;
			CheckResult( device->CreateDescriptorHeap( &rtvDesc, IID_PPV_ARGS( rtvHeap.GetAddressOf() ) ), "Failed to create the native RTV heap." );
			rtvDescriptorSize = device->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_RTV );

			D3D12_DESCRIPTOR_HEAP_DESC imguiDesc{};
			imguiDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			imguiDesc.NumDescriptors = ourImGuiDescriptorCapacity;
			imguiDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			CheckResult(
				device->CreateDescriptorHeap( &imguiDesc, IID_PPV_ARGS( imguiDescriptors.heap.GetAddressOf() ) ),
				"Failed to create the native ImGui SRV heap." );
			imguiDescriptors.descriptorSize = device->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );

			for( NativeFrame& frame : frames )
			{
				CheckResult(
					device->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS( frame.allocator.GetAddressOf() ) ),
					"Failed to create a native command allocator." );
			}
			CheckResult(
				device->CreateCommandList(
					0,
					D3D12_COMMAND_LIST_TYPE_DIRECT,
					frames[ 0 ].allocator.Get(),
					nullptr,
					IID_PPV_ARGS( commandList.GetAddressOf() ) ),
				"Failed to create the native command list." );
			CheckResult( commandList->Close(), "Failed to close the native command list." );

			CheckResult( device->CreateFence( 0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS( fence.GetAddressOf() ) ), "Failed to create the native fence." );
			fenceEvent = CreateEventW( nullptr, FALSE, FALSE, nullptr );
			if( fenceEvent == nullptr )
			{
				throw std::runtime_error( "Failed to create the native fence event." );
			}
			CreateBackbuffers();
		}

		void CreateBackbuffers()
		{
			D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtvHeap->GetCPUDescriptorHandleForHeapStart();
			for( uint32_t index = 0; index < backbuffers.size(); ++index )
			{
				CheckResult( swapchain->GetBuffer( index, IID_PPV_ARGS( backbuffers[ index ].GetAddressOf() ) ), "Failed to get a native backbuffer." );
				device->CreateRenderTargetView( backbuffers[ index ].Get(), nullptr, rtv );
				rtv.ptr += rtvDescriptorSize;
			}
		}

		void WaitForFrame( NativeFrame& frame )
		{
			if( frame.fenceValue == 0 || fence->GetCompletedValue() >= frame.fenceValue )
			{
				frame.fenceValue = 0;
				return;
			}
			CheckResult( fence->SetEventOnCompletion( frame.fenceValue, fenceEvent ), "Failed to wait for a native frame." );
			WaitForSingleObject( fenceEvent, INFINITE );
			frame.fenceValue = 0;
		}

		void WaitIdle()
		{
			if( queue == nullptr || fence == nullptr || fenceEvent == nullptr )
			{
				return;
			}
			const uint64_t fenceValue = ++nextFenceValue;
			CheckResult( queue->Signal( fence.Get(), fenceValue ), "Failed to signal the native queue." );
			if( fence->GetCompletedValue() < fenceValue )
			{
				CheckResult( fence->SetEventOnCompletion( fenceValue, fenceEvent ), "Failed to wait for the native queue." );
				WaitForSingleObject( fenceEvent, INFINITE );
			}
			for( NativeFrame& frame : frames )
			{
				frame.fenceValue = 0;
			}
		}

		void Resize( uint32_t width, uint32_t height )
		{
			if( width == 0 || height == 0 )
			{
				return;
			}
			WaitIdle();
			for( ComPtr<ID3D12Resource>& backbuffer : backbuffers )
			{
				backbuffer.Reset();
			}
			CheckResult(
				swapchain->ResizeBuffers( ourFramesInFlight, width, height, ourRenderTargetFormat, 0 ),
				"Failed to resize the native swapchain." );
			CreateBackbuffers();
		}

		NativeFrame& BeginFrame()
		{
			currentBackbuffer = swapchain->GetCurrentBackBufferIndex();
			NativeFrame& frame = frames[ currentBackbuffer ];
			WaitForFrame( frame );
			CheckResult( frame.allocator->Reset(), "Failed to reset the native command allocator." );
			CheckResult( commandList->Reset( frame.allocator.Get(), nullptr ), "Failed to reset the native command list." );
			return frame;
		}

		D3D12_CPU_DESCRIPTOR_HANDLE CurrentRtv() const noexcept
		{
			D3D12_CPU_DESCRIPTOR_HANDLE rtv = rtvHeap->GetCPUDescriptorHandleForHeapStart();
			rtv.ptr += static_cast<SIZE_T>( currentBackbuffer ) * rtvDescriptorSize;
			return rtv;
		}

		void SubmitFrame( NativeFrame& frame )
		{
			CheckResult( commandList->Close(), "Failed to close the native frame command list." );
			ID3D12CommandList* commandLists[] = { commandList.Get() };
			queue->ExecuteCommandLists( 1, commandLists );
			CheckResult( swapchain->Present( 1, 0 ), "Failed to present the native ImGui frame." );
			frame.fenceValue = ++nextFenceValue;
			CheckResult( queue->Signal( fence.Get(), frame.fenceValue ), "Failed to signal the native frame." );
		}

		NativeTexture CreateCheckerTexture()
		{
			std::array<uint32_t, 64u * 64u> pixels{};
			for( uint32_t y = 0; y < 64u; ++y )
			{
				for( uint32_t x = 0; x < 64u; ++x )
				{
					const bool white = ( ( x / 8u ) + ( y / 8u ) ) % 2u == 0u;
					pixels[ y * 64u + x ] = white ? 0xffffffffu : 0xff000000u;
				}
			}

			D3D12_RESOURCE_DESC textureDesc{};
			textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
			textureDesc.Width = 64;
			textureDesc.Height = 64;
			textureDesc.DepthOrArraySize = 1;
			textureDesc.MipLevels = 1;
			textureDesc.Format = ourRenderTargetFormat;
			textureDesc.SampleDesc.Count = 1;
			textureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

			D3D12_HEAP_PROPERTIES defaultHeap{};
			defaultHeap.Type = D3D12_HEAP_TYPE_DEFAULT;
			NativeTexture texture{};
			CheckResult(
				device->CreateCommittedResource(
					&defaultHeap,
					D3D12_HEAP_FLAG_NONE,
					&textureDesc,
					D3D12_RESOURCE_STATE_COPY_DEST,
					nullptr,
					IID_PPV_ARGS( texture.resource.GetAddressOf() ) ),
				"Failed to create the native checker texture." );

			D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
			uint32_t rowCount = 0;
			uint64_t rowSize = 0;
			uint64_t uploadSize = 0;
			device->GetCopyableFootprints( &textureDesc, 0, 1, 0, &footprint, &rowCount, &rowSize, &uploadSize );

			D3D12_RESOURCE_DESC uploadDesc{};
			uploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
			uploadDesc.Width = uploadSize;
			uploadDesc.Height = 1;
			uploadDesc.DepthOrArraySize = 1;
			uploadDesc.MipLevels = 1;
			uploadDesc.SampleDesc.Count = 1;
			uploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

			D3D12_HEAP_PROPERTIES uploadHeap{};
			uploadHeap.Type = D3D12_HEAP_TYPE_UPLOAD;
			ComPtr<ID3D12Resource> upload{};
			CheckResult(
				device->CreateCommittedResource(
					&uploadHeap,
					D3D12_HEAP_FLAG_NONE,
					&uploadDesc,
					D3D12_RESOURCE_STATE_GENERIC_READ,
					nullptr,
					IID_PPV_ARGS( upload.GetAddressOf() ) ),
				"Failed to create the native texture upload buffer." );

			uint8_t* mapped = nullptr;
			CheckResult( upload->Map( 0, nullptr, reinterpret_cast<void**>( &mapped ) ), "Failed to map the native texture upload buffer." );
			const uint8_t* source = reinterpret_cast<const uint8_t*>( pixels.data() );
			for( uint32_t row = 0; row < rowCount; ++row )
			{
				std::memcpy(
					mapped + footprint.Offset + static_cast<size_t>( row ) * footprint.Footprint.RowPitch,
					source + static_cast<size_t>( row ) * 64u * sizeof( uint32_t ),
					static_cast<size_t>( rowSize ) );
			}
			upload->Unmap( 0, nullptr );

			NativeFrame& uploadFrame = frames[ 0 ];
			WaitForFrame( uploadFrame );
			CheckResult( uploadFrame.allocator->Reset(), "Failed to reset the native upload allocator." );
			CheckResult( commandList->Reset( uploadFrame.allocator.Get(), nullptr ), "Failed to reset the native upload command list." );

			D3D12_TEXTURE_COPY_LOCATION destination{};
			destination.pResource = texture.resource.Get();
			destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

			D3D12_TEXTURE_COPY_LOCATION sourceLocation{};
			sourceLocation.pResource = upload.Get();
			sourceLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
			sourceLocation.PlacedFootprint = footprint;
			commandList->CopyTextureRegion( &destination, 0, 0, 0, &sourceLocation, nullptr );

			D3D12_RESOURCE_BARRIER barrier{};
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Transition.pResource = texture.resource.Get();
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
			barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			commandList->ResourceBarrier( 1, &barrier );
			CheckResult( commandList->Close(), "Failed to close the native upload command list." );

			ID3D12CommandList* commandLists[] = { commandList.Get() };
			queue->ExecuteCommandLists( 1, commandLists );
			WaitIdle();

			texture.descriptorIndex = imguiDescriptors.Allocate();
			D3D12_CPU_DESCRIPTOR_HANDLE cpu{};
			D3D12_GPU_DESCRIPTOR_HANDLE gpu{};
			imguiDescriptors.GetHandles( texture.descriptorIndex, cpu, gpu );

			D3D12_SHADER_RESOURCE_VIEW_DESC srv{};
			srv.Format = ourRenderTargetFormat;
			srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srv.Texture2D.MipLevels = 1;
			device->CreateShaderResourceView( texture.resource.Get(), &srv, cpu );
			texture.imguiTexture = ImTextureRef( static_cast<ImTextureID>( gpu.ptr ) );
			return texture;
		}

		void Shutdown() noexcept
		{
			if( queue != nullptr && fence != nullptr && fenceEvent != nullptr )
			{
				try
				{
					WaitIdle();
				}
				catch( ... )
				{
				}
			}
			for( ComPtr<ID3D12Resource>& backbuffer : backbuffers )
			{
				backbuffer.Reset();
			}
			if( fenceEvent != nullptr )
			{
				CloseHandle( fenceEvent );
				fenceEvent = nullptr;
			}
			commandList.Reset();
			for( NativeFrame& frame : frames )
			{
				frame.allocator.Reset();
			}
			fence.Reset();
			imguiDescriptors.heap.Reset();
			rtvHeap.Reset();
			swapchain.Reset();
			queue.Reset();
			device.Reset();
			factory.Reset();
		}
	};

	struct WindowState final
	{
		NativeRenderer* renderer = nullptr;
		uint32_t resizeWidth = 0;
		uint32_t resizeHeight = 0;
		bool resizePending = false;
		bool running = true;
		bool minimized = false;
	};

	LRESULT CALLBACK WindowProc( HWND window, UINT message, WPARAM wParam, LPARAM lParam )
	{
		if( ImGui::GetCurrentContext() != nullptr && ImGui_ImplWin32_WndProcHandler( window, message, wParam, lParam ) )
		{
			return 1;
		}

		WindowState* state = reinterpret_cast<WindowState*>( GetWindowLongPtr( window, GWLP_USERDATA ) );
		switch( message )
		{
			case WM_SIZE:
				if( state != nullptr )
				{
					state->resizeWidth = LOWORD( lParam );
					state->resizeHeight = HIWORD( lParam );
					state->minimized = wParam == SIZE_MINIMIZED || state->resizeWidth == 0 || state->resizeHeight == 0;
					state->resizePending = !state->minimized;
				}
				return 0;

			case WM_CLOSE:
				if( state != nullptr )
				{
					state->running = false;
				}
				return 0;

			case WM_DESTROY:
				PostQuitMessage( 0 );
				return 0;

			default:
				return DefWindowProc( window, message, wParam, lParam );
		}
	}
}

int WINAPI wWinMain( HINSTANCE instance, HINSTANCE, PWSTR, int showCommand )
{
	static constexpr wchar_t ourWindowClassName[] = L"ImGuiDemoNativeWindow";
	HWND window = nullptr;
	NativeRenderer renderer{};
	NativeTexture checkerTexture{};
	bool imguiWin32Initialized = false;
	bool imguiDx12Initialized = false;

	try
	{
		WNDCLASSEXW windowClass{};
		windowClass.cbSize = sizeof( windowClass );
		windowClass.lpfnWndProc = WindowProc;
		windowClass.hInstance = instance;
		windowClass.lpszClassName = ourWindowClassName;
		windowClass.hCursor = LoadCursor( nullptr, IDC_ARROW );
		if( RegisterClassExW( &windowClass ) == 0 )
		{
			throw std::runtime_error( "Failed to register the native ImGui window class." );
		}

		constexpr uint32_t initialWidth = 1280;
		constexpr uint32_t initialHeight = 720;
		window = CreateWindowExW(
			0,
			ourWindowClassName,
			L"Dear ImGui - Native DirectX 12",
			WS_OVERLAPPEDWINDOW,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			static_cast<int>( initialWidth ),
			static_cast<int>( initialHeight ),
			nullptr,
			nullptr,
			instance,
			nullptr );
		if( window == nullptr )
		{
			throw std::runtime_error( "Failed to create the native ImGui window." );
		}

		WindowState windowState{};
		windowState.renderer = &renderer;
		SetWindowLongPtr( window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>( &windowState ) );
		ShowWindow( window, showCommand );
		UpdateWindow( window );

		renderer.Initialize( window, initialWidth, initialHeight );
		checkerTexture = renderer.CreateCheckerTexture();

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsDark();

		if( !ImGui_ImplWin32_Init( window ) )
		{
			throw std::runtime_error( "ImGui Win32 initialization failed." );
		}
		imguiWin32Initialized = true;

		ImGui_ImplDX12_InitInfo imguiInfo{};
		imguiInfo.Device = renderer.device.Get();
		imguiInfo.CommandQueue = renderer.queue.Get();
		imguiInfo.NumFramesInFlight = ourFramesInFlight;
		imguiInfo.RTVFormat = ourRenderTargetFormat;
		imguiInfo.DSVFormat = DXGI_FORMAT_UNKNOWN;
		imguiInfo.SrvDescriptorHeap = renderer.imguiDescriptors.heap.Get();
		imguiInfo.UserData = &renderer.imguiDescriptors;
		imguiInfo.SrvDescriptorAllocFn = &AllocateImGuiDescriptor;
		imguiInfo.SrvDescriptorFreeFn = &FreeImGuiDescriptor;

		if( !ImGui_ImplDX12_Init( &imguiInfo ) )
		{
			throw std::runtime_error( "ImGui native DirectX 12 initialization failed." );
		}
		imguiDx12Initialized = true;

		int buttonPressCount = 0;
		float demoValue = 0.5f;
		MSG message{};
		while( windowState.running )
		{
			while( PeekMessage( &message, nullptr, 0, 0, PM_REMOVE ) )
			{
				if( message.message == WM_QUIT )
				{
					windowState.running = false;
					break;
				}
				TranslateMessage( &message );
				DispatchMessage( &message );
			}
			if( !windowState.running )
			{
				break;
			}
			if( windowState.minimized )
			{
				WaitMessage();
				continue;
			}
			if( windowState.resizePending )
			{
				renderer.Resize( windowState.resizeWidth, windowState.resizeHeight );
				windowState.resizePending = false;
			}

			ImGui_ImplDX12_NewFrame();
			ImGui_ImplWin32_NewFrame();

			ImGui::NewFrame();
			ImGui::Begin( "Native DirectX 12 + Dear ImGui" );
			ImGui::TextUnformatted( "This sample does not use Ldx12." );
			ImGui::TextUnformatted( "64x64 native D3D12 texture:" );
			ImGui::Image( checkerTexture.imguiTexture, ImVec2( 128.0f, 128.0f ) );
			ImGui::SliderFloat( "Value", &demoValue, 0.0f, 1.0f );
			if( ImGui::Button( "Press me" ) )
			{
				buttonPressCount++;
			}
			ImGui::SameLine();
			ImGui::Text( "Pressed %d times", buttonPressCount );

			ImGui::End();
			ImGui::Render();

			NativeFrame& frame = renderer.BeginFrame();
			ID3D12Resource* backbuffer = renderer.backbuffers[ renderer.currentBackbuffer ].Get();
			D3D12_RESOURCE_BARRIER beginBarrier{};
			beginBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			beginBarrier.Transition.pResource = backbuffer;
			beginBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
			beginBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
			beginBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			renderer.commandList->ResourceBarrier( 1, &beginBarrier );

			const D3D12_CPU_DESCRIPTOR_HANDLE rtv = renderer.CurrentRtv();
			const float clearColor[ 4 ] = { 0.04f, 0.05f, 0.08f, 1.0f };
			renderer.commandList->OMSetRenderTargets( 1, &rtv, FALSE, nullptr );
			renderer.commandList->ClearRenderTargetView( rtv, clearColor, 0, nullptr );
			ID3D12DescriptorHeap* descriptorHeaps[] = { renderer.imguiDescriptors.heap.Get() };
			renderer.commandList->SetDescriptorHeaps( 1, descriptorHeaps );
			ImGui_ImplDX12_RenderDrawData( ImGui::GetDrawData(), renderer.commandList.Get() );

			D3D12_RESOURCE_BARRIER endBarrier = beginBarrier;
			endBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			endBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
			renderer.commandList->ResourceBarrier( 1, &endBarrier );
			renderer.SubmitFrame( frame );
		}

		renderer.WaitIdle();
		ImGui_ImplDX12_Shutdown();
		imguiDx12Initialized = false;
		ImGui_ImplWin32_Shutdown();
		imguiWin32Initialized = false;
		ImGui::DestroyContext();

		checkerTexture.resource.Reset();
		renderer.imguiDescriptors.Free( checkerTexture.descriptorIndex );

		SetWindowLongPtr( window, GWLP_USERDATA, 0 );
		renderer.Shutdown();
		DestroyWindow( window );
		window = nullptr;
		UnregisterClassW( ourWindowClassName, instance );
		return 0;
	}
	catch( const std::exception& error )
	{
		if( imguiDx12Initialized )
		{
			ImGui_ImplDX12_Shutdown();
		}
		if( imguiWin32Initialized )
		{
			ImGui_ImplWin32_Shutdown();
		}
		if( ImGui::GetCurrentContext() != nullptr )
		{
			ImGui::DestroyContext();
		}
		checkerTexture.resource.Reset();
		renderer.Shutdown();
		if( window != nullptr && IsWindow( window ) != FALSE )
		{
			SetWindowLongPtr( window, GWLP_USERDATA, 0 );
			DestroyWindow( window );
		}
		UnregisterClassW( ourWindowClassName, instance );
		MessageBoxA( nullptr, error.what(), "ImGuiDemoNative", MB_ICONERROR | MB_OK );
		return 1;
	}
}
