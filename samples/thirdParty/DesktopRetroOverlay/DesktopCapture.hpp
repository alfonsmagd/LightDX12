#pragma once
#include <Ldx12/Ldx12.hpp>
#include <memory>
namespace desktop_retro
{
	// Windows Graphics Capture and all D3D11/WinRT implementation details live in the .cpp.
	// The render device must outlive capture. Call Shutdown after waiting for GPU work.
	class DesktopCapture final
	{
	public:
		enum class UpdateResult
		{
			NoNewFrame,
			FrameCopied,
			AccessLost
		};
		DesktopCapture();
		~DesktopCapture();
		DesktopCapture( const DesktopCapture& ) = delete;
		DesktopCapture& operator=( const DesktopCapture& ) = delete;
		void Initialize( ldx12::RenderDevice&, HMONITOR, HWND );
		void Recreate( ldx12::RenderDevice&, HMONITOR );
		UpdateResult Update( ldx12::RenderDevice& );
		void MarkCurrentTextureSubmitted( ldx12::RenderDevice& );
		void Shutdown( ldx12::RenderDevice& );
		ldx12::TextureHandle GetTexture() const noexcept;
		uint32_t GetWidth() const noexcept;
		uint32_t GetHeight() const noexcept;
		uint64_t GetArrivedFrameCount() const noexcept;
		uint64_t GetCopiedFrameCount() const noexcept;

	private:
		struct Impl;
		std::shared_ptr<Impl> impl_;
	};
}
