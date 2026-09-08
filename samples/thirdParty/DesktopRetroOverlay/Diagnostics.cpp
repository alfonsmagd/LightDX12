#include "Diagnostics.hpp"
#include <Ldx12/Ldx12Native.hpp>
#include <Windows.h>
#include <d3d12sdklayers.h>
#include <array>
#include <fstream>
#include <stdexcept>
#include <vector>

namespace desktop_retro
{
	std::filesystem::path FindShaderDirectory()
	{
		std::array<wchar_t, 32768> executablePath{};
		const DWORD length = GetModuleFileNameW( nullptr, executablePath.data(), static_cast<DWORD>( executablePath.size() ) );
		if( length == 0 || length >= executablePath.size() )
			throw std::runtime_error( "Could not locate the executable directory." );
		return std::filesystem::path( executablePath.data() ).parent_path() / "DesktopRetroOverlay";
	}

	void ThrowIfGpuReportedErrors( ldx12::RenderDevice& device )
	{
		device.WaitIdle();
		ldx12::ComPtr<ID3D12InfoQueue> messages;
		if( FAILED( device.GetNative().GetDevice()->QueryInterface( IID_PPV_ARGS( messages.GetAddressOf() ) ) ) )
			return;
		for( UINT64 index = 0; index < messages->GetNumStoredMessages(); ++index )
		{
			SIZE_T size = 0;
			messages->GetMessage( index, nullptr, &size );
			std::vector<unsigned char> storage( size );
			auto* message = reinterpret_cast<D3D12_MESSAGE*>( storage.data() );
			if( SUCCEEDED( messages->GetMessage( index, message, &size ) ) && message->Severity <= D3D12_MESSAGE_SEVERITY_ERROR )
				throw std::runtime_error( message->pDescription );
		}
	}

	int ReportFatalError( const std::string& message, bool silent )
	{
		std::ofstream( "DesktopRetroOverlay-error.txt", std::ios::trunc ) << message;
		if( !silent )
			MessageBoxA( nullptr, message.c_str(), "Desktop Retro Overlay", MB_ICONERROR | MB_OK );
		return EXIT_FAILURE;
	}
}
