#pragma once

#include "Ldx12/Ldx12.hpp"

#include <filesystem>

namespace ldx12
{
	class HLSLLoader final
	{
	public:
		static void SetRootDirectory( const std::filesystem::path& rootDirectory );
		static std::filesystem::path GetRootDirectory();
		static std::filesystem::path ResolvePath( const std::filesystem::path& shaderPath );
		static const char* LoadSource( const std::filesystem::path& shaderPath );
		static ShaderStageSource LoadStage( const std::filesystem::path& shaderPath, const char* profile, const char* entryPoint = "main" );
		static void ClearCache() noexcept;

	private:
		HLSLLoader() = delete;
	};
}
