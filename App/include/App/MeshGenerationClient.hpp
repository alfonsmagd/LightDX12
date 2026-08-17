#pragma once

#include "App/ConcurrentQueue.hpp"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace App
{
	class TaskSystem;

	enum class GenerationStage : uint8_t
	{
		Queued,
		Sending,
		Generating,
		Downloading,
		Completed,
		Failed
	};

	struct GenerationEvent
	{
		GenerationStage stage = GenerationStage::Queued;
		std::string jobId;
		std::string message;
		std::filesystem::path objPath;
	};

	struct MeshApiConfig
	{
		std::wstring host = L"127.0.0.1";
		uint16_t port = 8765;
		std::filesystem::path outputDirectory = L"C:\\GenerateIAMesh\\exports";
		uint32_t pollIntervalMilliseconds = 1000;
	};

	class MeshGenerationClient final
	{
	public:
		MeshGenerationClient( TaskSystem& tasks, MeshApiConfig config = {} );

		bool Submit( std::string prompt );
		bool IsBusy() const noexcept;
		std::optional<GenerationEvent> TryPopEvent();
		const MeshApiConfig& Config() const noexcept;

	private:
		void RunRequest( std::string prompt );
		void Publish( GenerationStage stage, std::string jobId, std::string message, std::filesystem::path path = {} );

		TaskSystem* tasks_ = nullptr;
		MeshApiConfig config_;
		ConcurrentQueue<GenerationEvent> events_;
		std::atomic<bool> busy_{ false };
	};
}
