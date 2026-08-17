#pragma once

#include "App/ConcurrentQueue.hpp"

#include <DirectXMath.h>

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace App
{
	class TaskSystem;

	struct MeshVertex
	{
		DirectX::XMFLOAT3 position{};
		DirectX::XMFLOAT3 normal{};
	};

	struct ObjMeshData
	{
		std::filesystem::path sourcePath;
		std::vector<MeshVertex> vertices;
		std::vector<uint32_t> indices;
		DirectX::XMFLOAT3 boundsMin{};
		DirectX::XMFLOAT3 boundsMax{};
		uint64_t fileBytes = 0;

		uint64_t CpuBytes() const noexcept;
		uint64_t GpuBytes() const noexcept;
		uint32_t TriangleCount() const noexcept;
	};

	ObjMeshData LoadObj( const std::filesystem::path& path );

	struct ObjCatalogEvent
	{
		std::vector<std::filesystem::path> files;
		std::string error;
	};

	struct ObjLoadEvent
	{
		std::optional<ObjMeshData> mesh;
		std::filesystem::path path;
		std::string error;
	};

	class ObjAssetService final
	{
	public:
		explicit ObjAssetService( TaskSystem& tasks );

		bool ScanAsync( std::vector<std::filesystem::path> roots );
		bool LoadAsync( std::filesystem::path path );
		bool IsScanning() const noexcept;
		bool IsLoading() const noexcept;
		std::optional<ObjCatalogEvent> TryPopCatalogEvent();
		std::optional<ObjLoadEvent> TryPopLoadEvent();

	private:
		TaskSystem* tasks_ = nullptr;
		ConcurrentQueue<ObjCatalogEvent> catalogEvents_;
		ConcurrentQueue<ObjLoadEvent> loadEvents_;
		std::atomic<bool> scanning_{ false };
		std::atomic<bool> loading_{ false };
	};
}
