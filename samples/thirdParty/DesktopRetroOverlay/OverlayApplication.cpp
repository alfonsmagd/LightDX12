#include "OverlayApplication.hpp"
#include "DesktopCapture.hpp"
#include "Diagnostics.hpp"
#include "OverlayControls.hpp"
#include "RetroRenderer.hpp"
#include "Win32Support.hpp"
#include <Ldx12/HLSLLoader.hpp>
#include <winrt/Windows.Graphics.Capture.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <stdexcept>
#include <string>

namespace desktop_retro
{
	namespace
	{
		using Clock = std::chrono::steady_clock;
		constexpr uint32_t kShortSmokeFrameCount = 8;
		constexpr uint32_t kSequenceSmokeFrameCount = 28;
		constexpr auto kSmokeTimeout = std::chrono::seconds( 15 );
	}

	struct OverlayApplication::Impl final
	{
		explicit Impl( HINSTANCE instance, AppOptions startupOptions ) : options( startupOptions )
		{
			Initialize( instance );
		}

		~Impl()
		{
			if( !renderDevice )
				return;
			try
			{
				renderDevice->WaitIdle();
				capture.Shutdown( *renderDevice );
			}
			catch( ... )
			{
				// The normal shutdown path is covered by smoke and integration tests.
			}
			renderer.reset();
			ldx12::DeviceManager::ShutdownSingleton();
		}

		void Initialize( HINSTANCE instance )
		{
			if( !winrt::Windows::Graphics::Capture::GraphicsCaptureSession::IsSupported() )
				throw std::runtime_error( "Windows Graphics Capture is unavailable." );

			SetThreadDpiAwarenessContext( DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 );
			controls.Initialize( instance, GetPrimaryMonitor(), options.smokeTest );
			controls.SelectPreset( options.initialPreset );

			const RECT bounds = GetMonitorRect( controls.Monitor() );
			ldx12::ContextDesc context{};
			context.preferHighPerformanceAdapter = false;
			context.enableDebugLayer =
#if defined( _DEBUG )
				true;
#else
				false;
#endif
			context.allowTearing = false;

			ldx12::SwapchainDesc swapchain{};
			swapchain.window = ldx12::MakeWin32WindowHandle( controls.Window() );
			swapchain.width = static_cast<uint32_t>( bounds.right - bounds.left );
			swapchain.height = static_cast<uint32_t>( bounds.bottom - bounds.top );
			swapchain.vsync = true;

			deviceManager = &ldx12::DeviceManager::Initialize( context, swapchain );
			renderDevice = deviceManager->GetRenderDevice();
			ldx12::HLSLLoader::SetRootDirectory( FindShaderDirectory() );
			renderer = std::make_unique<RetroRenderer>( *renderDevice, context.swapchainFormat );
			capture.Initialize( *renderDevice, controls.Monitor(), controls.Window() );
		}

		void Run()
		{
			startedAt = previousFrameAt = nextStatusAt = Clock::now();
			while( running )
				RunOneIteration();
			if( options.smokeTest )
				FinishSmokeTest();
		}

		void RunOneIteration()
		{
			CheckSmokeTimeout();
			controls.PumpMessages();
			const WindowEvents events = controls.ConsumeEvents();
			if( events.quitRequested )
			{
				running = false;
				return;
			}
			if( !controls.IsVisible() )
			{
				WaitMessage();
				previousFrameAt = Clock::now();
				return;
			}

			ApplyWindowEvents( events );
			AcquireAndRenderFrame();
		}

		void ApplyWindowEvents( const WindowEvents& events )
		{
			if( events.newWidth != 0 && events.newHeight != 0 )
			{
				deviceManager->Resize( events.newWidth, events.newHeight );
				renderer->ResetHistory();
			}
			if( events.captureRecreateRequested )
				RecreateCapture();
			if( events.effectHistoryResetRequested )
				renderer->ResetHistory();
		}

		void AcquireAndRenderFrame()
		{
			const DesktopCapture::UpdateResult result = capture.Update( *renderDevice );
			if( result == DesktopCapture::UpdateResult::AccessLost )
			{
				RecreateCapture();
				return;
			}
			if( !capture.GetTexture().Valid() )
			{
				Sleep( 1 );
				return;
			}
			if( waitingForFirstFrame )
			{
				controls.Reveal();
				waitingForFirstFrame = false;
			}

			RenderFrame();
			AdvanceSmokeSequence();
		}

		void RenderFrame()
		{
			const auto now = Clock::now();
			elapsedSeconds += std::clamp( std::chrono::duration<float>( now - previousFrameAt ).count(), 0.0f, 0.05f );
			previousFrameAt = now;

			ldx12::CommandBuffer& commands = renderDevice->AcquireCommandBuffer();
			const ldx12::TextureHandle backbuffer = renderDevice->GetCurrentSwapchainTexture();
			const CapturedFrame frame{ capture.GetTexture(), capture.GetWidth(), capture.GetHeight() };
			renderer->Record( commands, backbuffer, frame, controls.Settings(), elapsedSeconds );
			renderDevice->Submit( commands, backbuffer );
			capture.MarkCurrentTextureSubmitted( *renderDevice );
			++renderedFrameCount;

			if( !options.smokeTest && now >= nextStatusAt )
			{
				controls.SetStatusText( std::wstring( L"Desktop Retro Overlay | " ) + GetPreset( controls.Settings().preset ).name );
				nextStatusAt = now + std::chrono::seconds( 1 );
			}
		}

		void RecreateCapture()
		{
			renderDevice->WaitIdle();
			controls.ConcealUntilFirstFrame();
			waitingForFirstFrame = true;
			controls.MoveToMonitor( GetPrimaryMonitor() );
			capture.Recreate( *renderDevice, controls.Monitor() );
			renderer->ResetHistory();
			++captureRestartCount;
		}

		void AdvanceSmokeSequence()
		{
			if( !options.smokeTest )
				return;
			if( options.exerciseSequence )
			{
				if( renderedFrameCount % 4 == 0 && renderedFrameCount <= 16 )
					controls.SelectPreset( static_cast<RetroPreset>( ( renderedFrameCount / 4 ) % kPresets.size() ) );
				if( renderedFrameCount == 20 )
					RecreateCapture();
				if( renderedFrameCount == 24 )
				{
					renderer->ResetHistory();
					++historyResetCount;
				}
			}

			if( renderedFrameCount >= TargetSmokeFrameCount() )
				running = false;
		}

		uint32_t TargetSmokeFrameCount() const
		{
			return options.exerciseSequence ? kSequenceSmokeFrameCount : kShortSmokeFrameCount;
		}

		void CheckSmokeTimeout() const
		{
			if( options.smokeTest && Clock::now() - startedAt > kSmokeTimeout )
				throw std::runtime_error( "Capture/render test timed out after 15 seconds." );
		}

		void FinishSmokeTest()
		{
			if( renderedFrameCount < TargetSmokeFrameCount() )
				throw std::runtime_error( "Smoke test exited before completing its frames." );
			ThrowIfGpuReportedErrors( *renderDevice );
			std::ofstream( "DesktopRetroOverlay-status.txt", std::ios::trunc )
				<< "captured=" << capture.GetArrivedFrameCount() << " copied=" << capture.GetCopiedFrameCount() << " rendered=" << renderedFrameCount
				<< " preset=" << static_cast<uint32_t>( controls.Settings().preset ) << " captureRestarts=" << captureRestartCount
				<< " historyResets=" << historyResetCount;
		}

		AppOptions options;
		OverlayControls controls;
		DesktopCapture capture;
		ldx12::DeviceManager* deviceManager = nullptr;
		ldx12::RenderDevice* renderDevice = nullptr;
		std::unique_ptr<RetroRenderer> renderer;
		bool running = true;
		bool waitingForFirstFrame = true;
		Clock::time_point startedAt{};
		Clock::time_point previousFrameAt{};
		Clock::time_point nextStatusAt{};
		float elapsedSeconds = 0.0f;
		uint32_t renderedFrameCount = 0;
		uint32_t captureRestartCount = 0;
		uint32_t historyResetCount = 0;
	};

	OverlayApplication::OverlayApplication( HINSTANCE instance, AppOptions options ) : impl_( std::make_unique<Impl>( instance, options ) )
	{
	}
	OverlayApplication::~OverlayApplication() = default;
	void OverlayApplication::Run()
	{
		impl_->Run();
	}
}
