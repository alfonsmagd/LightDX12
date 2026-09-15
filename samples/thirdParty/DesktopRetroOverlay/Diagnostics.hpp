#pragma once

#include <Ldx12/Ldx12.hpp>
#include <filesystem>
#include <string>

namespace desktop_retro
{
	std::filesystem::path FindShaderDirectory();
	void ThrowIfGpuReportedErrors( ldx12::RenderDevice& device );
	int ReportFatalError( const std::string& message, bool silent );
}
