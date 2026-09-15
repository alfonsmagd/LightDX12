#include "DesktopCapture.hpp"
#include "Win32Support.hpp"
#include <Ldx12/Ldx12Native.hpp>
#include <d3d11.h>
#include <d3d11_4.h>
#include <shellapi.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>

#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <winrt/Windows.Foundation.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cwchar>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>

namespace desktop_retro
{
	using namespace ldx12;
	struct DesktopCapture::Impl final : std::enable_shared_from_this<DesktopCapture::Impl>
	{
	public:
		void Initialize( RenderDevice& renderDevice, HMONITOR monitor, HWND statusWindow )
		{
			statusWindow_ = statusWindow;
			SetWindowTextW( statusWindow_, L"Ldx12 Desktop Retro Overlay | initializing native capture device" );
			ComPtr<IDXGIFactory6> factory;
			ThrowIfFailed( CreateDXGIFactory1( IID_PPV_ARGS( factory.GetAddressOf() ) ),
				"Failed to create the DXGI factory used to select the capture adapter." );

			const LUID targetAdapterLuid = renderDevice.GetNative().GetDevice()->GetAdapterLuid();
			ComPtr<IDXGIAdapter1> adapter;
			for( UINT adapterIndex = 0;; ++adapterIndex )
			{
				ComPtr<IDXGIAdapter1> candidate;
				const HRESULT result = factory->EnumAdapters1( adapterIndex, candidate.GetAddressOf() );
				if( result == DXGI_ERROR_NOT_FOUND )
				{
					break;
				}
				ThrowIfFailed( result, "Failed to enumerate the graphics adapters." );

				DXGI_ADAPTER_DESC1 candidateDesc{};
				ThrowIfFailed( candidate->GetDesc1( &candidateDesc ), "Failed to inspect a graphics adapter." );
				if( candidateDesc.AdapterLuid.HighPart == targetAdapterLuid.HighPart && candidateDesc.AdapterLuid.LowPart == targetAdapterLuid.LowPart )
				{
					adapter = std::move( candidate );
					break;
				}
			}
			if( !adapter )
			{
				throw std::runtime_error( "Could not find the Direct3D 12 adapter for native Windows Graphics Capture." );
			}

			ThrowIfFailed( D3D11CreateDevice( adapter.Get(),
							   D3D_DRIVER_TYPE_UNKNOWN,
							   nullptr,
							   D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_VIDEO_SUPPORT,
							   nullptr,
							   0,
							   D3D11_SDK_VERSION,
							   d3d11Device_.GetAddressOf(),
							   nullptr,
							   d3d11Context_.GetAddressOf() ),
				"Failed to create the native D3D11 device used by Windows Graphics Capture." );

			ThrowIfFailed( d3d11Device_.As( &d3d11Device5_ ), "The capture device does not support Direct3D 11 shared fences." );
			ThrowIfFailed( d3d11Context_.As( &d3d11Context4_ ), "The capture context does not support Direct3D 11 shared fences." );
			ThrowIfFailed( d3d11Device5_->CreateFence( 0, D3D11_FENCE_FLAG_SHARED, IID_PPV_ARGS( captureFence11_.GetAddressOf() ) ),
				"Failed to create the Direct3D 11 capture fence." );
			HANDLE captureFenceHandle = nullptr;
			ThrowIfFailed( captureFence11_->CreateSharedHandle( nullptr, GENERIC_ALL, nullptr, &captureFenceHandle ),
				"Failed to share the Direct3D 11 capture fence." );
			const HRESULT openCaptureFenceResult =
				renderDevice.GetNative().GetDevice()->OpenSharedHandle( captureFenceHandle, IID_PPV_ARGS( captureFence12_.GetAddressOf() ) );
			CloseHandle( captureFenceHandle );
			ThrowIfFailed( openCaptureFenceResult, "Direct3D 12 could not open the capture synchronization fence." );
			ThrowIfFailed( renderDevice.GetNative().GetDevice()->CreateFence( 0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS( renderFence_.GetAddressOf() ) ),
				"Failed to create the Direct3D 12 fence for shared desktop textures." );

			ComPtr<IDXGIDevice> dxgiDevice;
			ThrowIfFailed( d3d11Device_.As( &dxgiDevice ), "Failed to query the DXGI device used by Windows Graphics Capture." );
			ComPtr<IInspectable> inspectableDevice;
			ThrowIfFailed( CreateDirect3D11DeviceFromDXGIDevice( dxgiDevice.Get(), inspectableDevice.GetAddressOf() ),
				"Failed to create the Windows Runtime capture device." );
			captureDevice_ = { inspectableDevice.Detach(), winrt::take_ownership_from_abi };
			SetWindowTextW( statusWindow_, L"Ldx12 Desktop Retro Overlay | preparing native capture session" );
			Recreate( renderDevice, monitor );
			SetWindowTextW( statusWindow_, L"Ldx12 Desktop Retro Overlay | waiting for capture frame" );
		}

		void Recreate( RenderDevice& renderDevice, HMONITOR monitor )
		{
			if( framePool_ && frameArrivedRegistrationActive_ )
			{
				framePool_.FrameArrived( frameArrivedToken_ );
				frameArrivedRegistrationActive_ = false;
			}
			{
				std::scoped_lock lock( pendingFrameMutex_ );
				++sessionGeneration_;
				pendingCaptureTexture_.Reset();
			}
			if( captureSession_ )
			{
				captureSession_.Close();
			}
			if( framePool_ )
			{
				framePool_.Close();
			}
			captureItem_ = nullptr;
			DestroySharedTextures( renderDevice );

			SetWindowTextW( statusWindow_, L"Ldx12 Desktop Retro Overlay | selecting monitor for native capture" );
			const auto captureItemInterop =
				winrt::get_activation_factory<winrt::Windows::Graphics::Capture::GraphicsCaptureItem, IGraphicsCaptureItemInterop>();
			ThrowIfFailed( captureItemInterop->CreateForMonitor( monitor,
							   winrt::guid_of<winrt::Windows::Graphics::Capture::GraphicsCaptureItem>(),
							   winrt::put_abi( captureItem_ ) ),
				"Windows Graphics Capture could not create a monitor capture item." );

			const winrt::Windows::Graphics::SizeInt32 size = captureItem_.Size();
			const uint32_t width = static_cast<uint32_t>( size.Width );
			const uint32_t height = static_cast<uint32_t>( size.Height );
			if( width == 0 || height == 0 )
			{
				throw std::runtime_error( "The selected monitor has an invalid size." );
			}

			SetWindowTextW( statusWindow_, L"Ldx12 Desktop Retro Overlay | creating shared textures" );
			CreateSharedTextures( renderDevice, width, height );
			width_ = width;
			height_ = height;

			SetWindowTextW( statusWindow_, L"Ldx12 Desktop Retro Overlay | creating native frame pool" );
			framePool_ = winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::CreateFreeThreaded( captureDevice_,
				winrt::Windows::Graphics::DirectX::DirectXPixelFormat::B8G8R8A8UIntNormalized,
				2,
				size );
			frameArrivedToken_ = framePool_.FrameArrived(
				[ weak = weak_from_this(), generation = sessionGeneration_.load(), expectedWidth = width_, expectedHeight = height_ ]( const auto& sender,
					const auto& ) noexcept
				{
					const auto self = weak.lock();
					if( !self )
						return;
					try
					{
						auto frame = sender.TryGetNextFrame();
						if( !frame )
						{
							return;
						}

						const winrt::Windows::Graphics::SizeInt32 frameSize = frame.ContentSize();
						if( frameSize.Width != static_cast<int32_t>( expectedWidth ) || frameSize.Height != static_cast<int32_t>( expectedHeight ) )
						{
							if( generation == self->sessionGeneration_.load() )
								self->pendingResize_.store( true, std::memory_order_release );
							return;
						}

						ComPtr<::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess> dxgiInterfaceAccess;
						auto surface = frame.Surface();
						ThrowIfFailed( reinterpret_cast<IInspectable*>( winrt::get_abi( surface ) )
										   ->QueryInterface( IID_PPV_ARGS( dxgiInterfaceAccess.GetAddressOf() ) ),
							"Windows Graphics Capture returned a surface without DXGI access." );

						ComPtr<ID3D11Texture2D> capturedTexture;
						ThrowIfFailed( dxgiInterfaceAccess->GetInterface( IID_PPV_ARGS( capturedTexture.GetAddressOf() ) ),
							"Windows Graphics Capture did not return a D3D11 texture." );

						// A Direct3D11CaptureFrame must leave this callback before the frame-pool
						// slot is reusable. Keep only its underlying D3D texture, as ShaderGlass
						// does, rather than retaining the WinRT frame object across callbacks.
						std::scoped_lock lock( self->pendingFrameMutex_ );
						if( generation != self->sessionGeneration_.load() )
							return;
						self->pendingCaptureTexture_ = std::move( capturedTexture );
						self->arrivedFrameCount_.fetch_add( 1, std::memory_order_relaxed );
					}
					catch( ... )
					{
						// A capture callback must never propagate a WinRT exception.
					}
				} );
			frameArrivedRegistrationActive_ = true;
			SetWindowTextW( statusWindow_, L"Ldx12 Desktop Retro Overlay | creating native capture session" );
			captureSession_ = framePool_.CreateCaptureSession( captureItem_ );
			if( const auto session2 = captureSession_.try_as<winrt::Windows::Graphics::Capture::IGraphicsCaptureSession2>() )
			{
				session2.IsCursorCaptureEnabled( false );
			}
			if( const auto session3 = captureSession_.try_as<winrt::Windows::Graphics::Capture::IGraphicsCaptureSession3>() )
			{
				try
				{
					session3.IsBorderRequired( false );
				}
				catch( ... )
				{
					// Windows may refuse this request on older builds or protected content.
				}
			}
			SetWindowTextW( statusWindow_, L"Ldx12 Desktop Retro Overlay | starting native capture" );
			captureSession_.StartCapture();
			SetWindowTextW( statusWindow_, L"Ldx12 Desktop Retro Overlay | native capture started" );
		}

		UpdateResult Update( RenderDevice& renderDevice )
		{
			if( pendingResize_.exchange( false, std::memory_order_acq_rel ) )
			{
				return UpdateResult::AccessLost;
			}

			ComPtr<ID3D11Texture2D> capturedTexture;
			{
				std::scoped_lock lock( pendingFrameMutex_ );
				if( !pendingCaptureTexture_ )
				{
					return UpdateResult::NoNewFrame;
				}
				capturedTexture = std::move( pendingCaptureTexture_ );
			}

			// Never stall the desktop path waiting for a texture that D3D12 is still
			// presenting. Use the next idle texture, or drop this frame and keep the
			// latest displayed one. That prevents capture latency from accumulating.
			SharedCaptureTexture* destination = nullptr;
			uint32_t destinationIndex = kNoTexture;
			for( uint32_t attempt = 0; attempt < sharedTextures_.size(); ++attempt )
			{
				const uint32_t candidateIndex = ( nextWriteTexture_ + attempt ) % sharedTextures_.size();
				SharedCaptureTexture& candidate = sharedTextures_[ candidateIndex ];
				if( !IsReadyForD3D11Write( candidate ) )
				{
					continue;
				}

				const HRESULT acquireResult = candidate.keyedMutex->AcquireSync( 0, 0 );
				if( acquireResult == static_cast<HRESULT>( WAIT_TIMEOUT ) )
				{
					continue;
				}
				ThrowIfFailed( acquireResult, "Failed to acquire the shared desktop capture texture." );
				destination = &candidate;
				destinationIndex = candidateIndex;
				break;
			}
			if( destination == nullptr )
			{
				return UpdateResult::NoNewFrame;
			}

			d3d11Context_->CopyResource( destination->d3d11Texture.Get(), capturedTexture.Get() );
			const uint64_t captureFenceValue = nextCaptureFenceValue_++;
			ThrowIfFailed( d3d11Context4_->Signal( captureFence11_.Get(), captureFenceValue ), "Failed to signal that a captured desktop frame is ready." );
			d3d11Context_->Flush();
			ThrowIfFailed( destination->keyedMutex->ReleaseSync( 0 ), "Failed to release the shared desktop capture texture." );
			ThrowIfFailed( renderDevice.GetNative().GetCommandQueue()->Wait( captureFence12_.Get(), captureFenceValue ),
				"Direct3D 12 could not wait for the captured desktop frame." );
			currentTexture_ = destinationIndex;
			nextWriteTexture_ = ( destinationIndex + 1u ) % sharedTextures_.size();
			copiedFrameCount_.fetch_add( 1, std::memory_order_relaxed );
			return UpdateResult::FrameCopied;
		}

		void MarkCurrentTextureSubmitted( RenderDevice& renderDevice )
		{
			if( currentTexture_ == kNoTexture )
			{
				return;
			}

			SharedCaptureTexture& texture = sharedTextures_[ currentTexture_ ];
			const uint64_t fenceValue = nextRenderFenceValue_++;
			ThrowIfFailed( renderDevice.GetNative().GetCommandQueue()->Signal( renderFence_.Get(), fenceValue ),
				"Failed to signal the desktop texture synchronization fence." );
			texture.lastD3D12UseFenceValue = fenceValue;
		}

		void Shutdown( RenderDevice& renderDevice )
		{
			if( framePool_ && frameArrivedRegistrationActive_ )
			{
				framePool_.FrameArrived( frameArrivedToken_ );
				frameArrivedRegistrationActive_ = false;
			}
			{
				std::scoped_lock lock( pendingFrameMutex_ );
				++sessionGeneration_;
				pendingCaptureTexture_.Reset();
			}
			if( captureSession_ )
			{
				captureSession_.Close();
			}
			if( framePool_ )
			{
				framePool_.Close();
			}
			captureSession_ = nullptr;
			framePool_ = nullptr;
			captureItem_ = nullptr;
			DestroySharedTextures( renderDevice );
			captureFence12_.Reset();
			captureFence11_.Reset();
			d3d11Context4_.Reset();
			d3d11Device5_.Reset();
			d3d11Context_.Reset();
			d3d11Device_.Reset();
			captureDevice_ = nullptr;
			renderFence_.Reset();
			width_ = 0;
			height_ = 0;
		}

		TextureHandle GetTexture() const noexcept
		{
			return currentTexture_ == kNoTexture ? TextureHandle{} : sharedTextures_[ currentTexture_ ].d3d12Texture;
		}
		uint32_t GetWidth() const noexcept
		{
			return width_;
		}
		uint32_t GetHeight() const noexcept
		{
			return height_;
		}
		uint64_t GetArrivedFrameCount() const noexcept
		{
			return arrivedFrameCount_.load( std::memory_order_relaxed );
		}
		uint64_t GetCopiedFrameCount() const noexcept
		{
			return copiedFrameCount_.load( std::memory_order_relaxed );
		}

	private:
		static constexpr uint32_t kSharedTextureCount = 3;
		static constexpr uint32_t kNoTexture = UINT32_MAX;

		struct SharedCaptureTexture final
		{
			ComPtr<ID3D11Texture2D> d3d11Texture;
			ComPtr<IDXGIKeyedMutex> keyedMutex;
			TextureHandle d3d12Texture = {};
			uint64_t lastD3D12UseFenceValue = 0;
		};

		void CreateSharedTextures( RenderDevice& renderDevice, uint32_t width, uint32_t height )
		{
			for( SharedCaptureTexture& texture : sharedTextures_ )
			{
				D3D11_TEXTURE2D_DESC desc{};
				desc.Width = width;
				desc.Height = height;
				desc.MipLevels = 1;
				desc.ArraySize = 1;
				desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
				desc.SampleDesc.Count = 1;
				desc.Usage = D3D11_USAGE_DEFAULT;
				desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
				desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_NTHANDLE | D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
				ThrowIfFailed( d3d11Device_->CreateTexture2D( &desc, nullptr, texture.d3d11Texture.GetAddressOf() ),
					"Failed to create a shared desktop capture texture." );
				ThrowIfFailed( texture.d3d11Texture.As( &texture.keyedMutex ), "Failed to query the shared desktop texture mutex." );

				ComPtr<IDXGIResource1> dxgiTexture;
				ThrowIfFailed( texture.d3d11Texture.As( &dxgiTexture ), "Failed to query the shared desktop texture." );
				HANDLE sharedHandle = nullptr;
				ThrowIfFailed( dxgiTexture->CreateSharedHandle( nullptr, DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr, &sharedHandle ),
					"Failed to create a shared handle for the desktop capture texture." );

				ComPtr<ID3D12Resource> d3d12Texture;
				const HRESULT openResult = renderDevice.GetNative().GetDevice()->OpenSharedHandle( sharedHandle, IID_PPV_ARGS( d3d12Texture.GetAddressOf() ) );
				CloseHandle( sharedHandle );
				ThrowIfFailed( openResult, "Direct3D 12 could not open the shared desktop capture texture." );
				texture.d3d12Texture = renderDevice.GetNative().ImportSampledTexture2D( d3d12Texture.Get() );
			}
			currentTexture_ = kNoTexture;
			nextWriteTexture_ = 0;
		}

		void DestroySharedTextures( RenderDevice& renderDevice )
		{
			for( SharedCaptureTexture& texture : sharedTextures_ )
			{
				if( texture.d3d12Texture.Valid() )
				{
					renderDevice.Destroy( texture.d3d12Texture );
					texture.d3d12Texture = {};
				}
				texture.d3d11Texture.Reset();
				texture.keyedMutex.Reset();
				texture.lastD3D12UseFenceValue = 0;
			}
			currentTexture_ = kNoTexture;
			nextWriteTexture_ = 0;
		}

		bool IsReadyForD3D11Write( const SharedCaptureTexture& texture ) const noexcept
		{
			return texture.lastD3D12UseFenceValue == 0 || renderFence_->GetCompletedValue() >= texture.lastD3D12UseFenceValue;
		}

		ComPtr<ID3D11Device> d3d11Device_;
		ComPtr<ID3D11DeviceContext> d3d11Context_;
		ComPtr<ID3D11Device5> d3d11Device5_;
		ComPtr<ID3D11DeviceContext4> d3d11Context4_;
		ComPtr<ID3D11Fence> captureFence11_;
		ComPtr<ID3D12Fence> captureFence12_;
		ComPtr<ID3D12Fence> renderFence_;
		winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice captureDevice_ = nullptr;
		winrt::Windows::Graphics::Capture::GraphicsCaptureItem captureItem_ = nullptr;
		winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool framePool_ = nullptr;
		winrt::Windows::Graphics::Capture::GraphicsCaptureSession captureSession_ = nullptr;
		winrt::event_token frameArrivedToken_ = {};
		std::mutex pendingFrameMutex_;
		std::atomic_uint64_t sessionGeneration_ = 0;
		ComPtr<ID3D11Texture2D> pendingCaptureTexture_;
		std::atomic_uint64_t arrivedFrameCount_ = 0;
		std::atomic_bool pendingResize_ = false;
		bool frameArrivedRegistrationActive_ = false;
		std::array<SharedCaptureTexture, kSharedTextureCount> sharedTextures_ = {};
		uint32_t currentTexture_ = kNoTexture;
		uint32_t nextWriteTexture_ = 0;
		uint64_t nextCaptureFenceValue_ = 1;
		uint64_t nextRenderFenceValue_ = 1;
		uint32_t width_ = 0;
		uint32_t height_ = 0;
		std::atomic_uint64_t copiedFrameCount_ = 0;
		HWND statusWindow_ = nullptr;
	};

	DesktopCapture::DesktopCapture() : impl_( std::make_shared<Impl>() )
	{
	}
	DesktopCapture::~DesktopCapture() = default;
	void DesktopCapture::Initialize( RenderDevice& d, HMONITOR m, HWND w )
	{
		impl_->Initialize( d, m, w );
	}
	void DesktopCapture::Recreate( RenderDevice& d, HMONITOR m )
	{
		impl_->Recreate( d, m );
	}
	DesktopCapture::UpdateResult DesktopCapture::Update( RenderDevice& d )
	{
		return impl_->Update( d );
	}
	void DesktopCapture::MarkCurrentTextureSubmitted( RenderDevice& d )
	{
		impl_->MarkCurrentTextureSubmitted( d );
	}
	void DesktopCapture::Shutdown( RenderDevice& d )
	{
		impl_->Shutdown( d );
	}
	TextureHandle DesktopCapture::GetTexture() const noexcept
	{
		return impl_->GetTexture();
	}
	uint32_t DesktopCapture::GetWidth() const noexcept
	{
		return impl_->GetWidth();
	}
	uint32_t DesktopCapture::GetHeight() const noexcept
	{
		return impl_->GetHeight();
	}
	uint64_t DesktopCapture::GetArrivedFrameCount() const noexcept
	{
		return impl_->GetArrivedFrameCount();
	}
	uint64_t DesktopCapture::GetCopiedFrameCount() const noexcept
	{
		return impl_->GetCopiedFrameCount();
	}
}
