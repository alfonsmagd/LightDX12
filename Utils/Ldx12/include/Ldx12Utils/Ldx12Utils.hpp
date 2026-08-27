#pragma once

#include "Ldx12/Ldx12.hpp"

#include <DirectXMath.h>

#include <array>
#include <cstdint>
#include <limits>
#include <vector>

namespace ldx12::utils
{
	struct Transform
	{
		DirectX::XMFLOAT3 position = {};
		DirectX::XMFLOAT3 rotation = {};
		DirectX::XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f };
	};

	struct CubeDesc
	{
		Transform transform = {};
		DirectX::XMFLOAT3 size = { 1.0f, 1.0f, 1.0f };
	};

	struct SphereDesc
	{
		Transform transform = {};
		float radius = 0.5f;
		uint32_t longitudeSegments = 24;
		uint32_t latitudeSegments = 16;
	};

	struct Camera
	{
		DirectX::XMFLOAT3 position = { 0.0f, 2.0f, -7.0f };
		DirectX::XMFLOAT3 target = {};
		DirectX::XMFLOAT3 up = { 0.0f, 1.0f, 0.0f };
		float verticalFieldOfViewRadians = 1.04719755f;
		float aspectRatio = 16.0f / 9.0f;
		float nearPlane = 0.1f;
		float farPlane = 1000.0f;
	};

	class MeshHandle final
	{
	public:
		MeshHandle() = default;

		[[nodiscard]] bool Valid() const noexcept { return generation_ != 0; }
		[[nodiscard]] uint32_t Index() const noexcept { return index_; }
		[[nodiscard]] uint32_t Generation() const noexcept { return generation_; }
		[[nodiscard]] bool operator==( const MeshHandle& other ) const noexcept { return index_ == other.index_ && generation_ == other.generation_; }
		[[nodiscard]] bool operator!=( const MeshHandle& other ) const noexcept { return !(*this == other); }

	private:
		friend class World;

		MeshHandle( uint32_t index, uint32_t generation ) noexcept: index_( index ), generation_( generation ) {}

		uint32_t index_ = 0;
		uint32_t generation_ = 0;
	};

	class World final
	{
	public:
		MeshHandle AddCube( const CubeDesc& desc );
		MeshHandle AddSphere( const SphereDesc& desc );
		bool SetTransform( MeshHandle mesh, const Transform& transform );
		bool Destroy( MeshHandle mesh );
		void Clear();

		[[nodiscard]] bool Contains( MeshHandle mesh ) const noexcept;
		[[nodiscard]] uint32_t NumMeshes() const noexcept { return meshCount_; }

	private:
		friend class RenderWorld;

		enum class GeometryType : uint8_t
		{
			Cube,
			Sphere,
		};

		struct Object
		{
			GeometryType geometry = GeometryType::Cube;
			Transform transform = {};
			DirectX::XMFLOAT3 cubeSize = { 1.0f, 1.0f, 1.0f };
			float sphereRadius = 0.5f;
			uint32_t longitudeSegments = 24;
			uint32_t latitudeSegments = 16;
		};

		struct Slot
		{
			Object object = {};
			uint32_t generation = 1;
			bool occupied = false;
		};

		MeshHandle AddObject( Object&& object );
		Object* GetObject( MeshHandle mesh ) noexcept;
		static void IncrementGeneration( Slot& slot ) noexcept;

		std::vector<Slot> objects_;
		std::vector<uint32_t> freeSlots_;
		uint64_t geometryRevision_ = 1;
		uint64_t transformRevision_ = 1;
		uint32_t meshCount_ = 0;
	};

	struct RenderWorldDesc
	{
		DXGI_FORMAT colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		DXGI_FORMAT depthFormat = DXGI_FORMAT_D32_FLOAT;
	};

	class RenderWorld final
	{
	public:
		RenderWorld( RenderDevice& device, const RenderWorldDesc& desc = {} );
		~RenderWorld();

		RenderWorld( const RenderWorld& ) = delete;
		RenderWorld& operator=( const RenderWorld& ) = delete;
		RenderWorld( RenderWorld&& ) = delete;
		RenderWorld& operator=( RenderWorld&& ) = delete;

		void Render( ICommandBuffer& commands, const World& world, const Camera& camera );

		[[nodiscard]] uint32_t GetVertexCount() const noexcept { return static_cast<uint32_t>( vertices_.size() ); }
		[[nodiscard]] uint32_t GetIndexCount() const noexcept { return indexCount_; }
		[[nodiscard]] uint32_t GetDrawCount() const noexcept { return drawCount_; }

	private:
		struct Vertex
		{
			DirectX::XMFLOAT3 position = {};
		};

		struct IndirectDraw
		{
			uint32_t baseInstance = 0;
			D3D12_DRAW_INDEXED_ARGUMENTS arguments = {};
		};

		struct GpuTransform
		{
			std::array<float, 16> model = {};
		};

		struct PushConstants
		{
			std::array<float, 16> viewProjection = {};
			uint32_t transformBufferIndex = 0;
		};

		void Synchronize( const World& world );
		void RebuildGeometry( const World& world );
		void RebuildTransforms( const World& world );
		void AppendCube( const World::Object& object );
		void AppendSphere( const World::Object& object );
		void UploadVertexBuffer();
		void UploadIndexBuffer();
		void UploadTransformBuffer();
		void UploadIndirectBuffer();
		void RetireBuffer( BufferHandle& buffer );
		void ReleaseBuffers();
		static uint64_t GrowCapacity( uint64_t requiredSize );
		static GpuTransform BuildGpuTransform( const Transform& transform );
		static PushConstants BuildPushConstants( RenderDevice& device, BufferHandle transformBuffer, const Camera& camera );

		RenderDevice* device_ = nullptr;
		RenderPipelineState pipeline_;
		BufferHandle vertexBuffer_ = {};
		BufferHandle indexBuffer_ = {};
		BufferHandle transformBuffer_ = {};
		BufferHandle indirectBuffer_ = {};
		std::vector<BufferHandle> retiredBuffers_;
		std::vector<Vertex> vertices_;
		std::vector<uint32_t> indices_;
		std::vector<GpuTransform> transforms_;
		std::vector<IndirectDraw> indirectDraws_;
		std::vector<uint32_t> objectIndices_;
		const World* synchronizedWorld_ = nullptr;
		uint64_t vertexBufferCapacity_ = 0;
		uint64_t indexBufferCapacity_ = 0;
		uint64_t transformBufferCapacity_ = 0;
		uint64_t indirectBufferCapacity_ = 0;
		uint64_t uploadedGeometryRevision_ = std::numeric_limits<uint64_t>::max();
		uint64_t uploadedTransformRevision_ = std::numeric_limits<uint64_t>::max();
		uint32_t indexCount_ = 0;
		uint32_t drawCount_ = 0;
	};
}
