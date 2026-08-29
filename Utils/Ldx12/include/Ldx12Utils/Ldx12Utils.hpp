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
		DirectX::XMFLOAT4 color = { 0.20f, 0.65f, 1.00f, 1.0f };
		bool wireframe = false;
	};

	struct SphereDesc
	{
		Transform transform = {};
		float radius = 0.5f;
		DirectX::XMFLOAT4 color = { 1.00f, 0.75f, 0.20f, 1.0f };
		bool wireframe = true;
	};

	struct ArrowDesc
	{
		DirectX::XMFLOAT3 start = {};
		DirectX::XMFLOAT3 end = { 0.0f, 1.0f, 0.0f };
		DirectX::XMFLOAT4 color = { 1.0f, 0.25f, 0.10f, 1.0f };
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

	class ObjectHandle final
	{
	public:
		ObjectHandle() = default;

		[[nodiscard]] bool Valid() const noexcept { return generation_ != 0; }
		[[nodiscard]] uint32_t Index() const noexcept { return index_; }
		[[nodiscard]] uint32_t Generation() const noexcept { return generation_; }
		[[nodiscard]] bool operator==( const ObjectHandle& other ) const noexcept { return index_ == other.index_ && generation_ == other.generation_; }
		[[nodiscard]] bool operator!=( const ObjectHandle& other ) const noexcept { return !(*this == other); }

	private:
		friend class World;

		ObjectHandle( uint32_t index, uint32_t generation ) noexcept: index_( index ), generation_( generation ) {}

		uint32_t index_ = 0;
		uint32_t generation_ = 0;
	};

	class World final
	{
	public:
		ObjectHandle AddCube( const CubeDesc& desc );
		ObjectHandle AddSphere( const SphereDesc& desc );
		ObjectHandle AddArrow( const ArrowDesc& desc );
		bool SetTransform( ObjectHandle object, const Transform& transform );
		bool Destroy( ObjectHandle object );
		void Clear();

		[[nodiscard]] bool Contains( ObjectHandle object ) const noexcept;
		[[nodiscard]] uint32_t NumObjects() const noexcept { return objectCount_; }

	private:
		friend class DebugRenderer;

		enum class PrimitiveType : uint8_t
		{
			Cube,
			Sphere,
			Arrow,
			Count,
		};

		struct Object
		{
			PrimitiveType primitive = PrimitiveType::Cube;
			Transform transform = {};
			DirectX::XMFLOAT3 cubeSize = { 1.0f, 1.0f, 1.0f };
			float sphereRadius = 0.5f;
			DirectX::XMFLOAT3 arrowStart = {};
			DirectX::XMFLOAT3 arrowEnd = { 0.0f, 1.0f, 0.0f };
			DirectX::XMFLOAT4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
			bool wireframe = false;
		};

		struct Slot
		{
			Object object = {};
			uint32_t generation = 1;
			bool occupied = false;
		};

		ObjectHandle AddObject( Object&& object );
		Object* GetObject( ObjectHandle object ) noexcept;
		static void IncrementGeneration( Slot& slot ) noexcept;

		std::vector<Slot> objects_;
		std::vector<uint32_t> freeSlots_;
		uint64_t objectRevision_ = 1;
		uint64_t transformRevision_ = 1;
		uint32_t objectCount_ = 0;
	};

	struct DebugRendererDesc
	{
		DXGI_FORMAT colorFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
		DXGI_FORMAT depthFormat = DXGI_FORMAT_D32_FLOAT;
	};

	class DebugRenderer final
	{
	public:
		DebugRenderer( RenderDevice& device, const DebugRendererDesc& desc = {} );
		~DebugRenderer();

		DebugRenderer( const DebugRenderer& ) = delete;
		DebugRenderer& operator=( const DebugRenderer& ) = delete;
		DebugRenderer( DebugRenderer&& ) = delete;
		DebugRenderer& operator=( DebugRenderer&& ) = delete;

		void Render( ICommandBuffer& commands, const World& world, const Camera& camera );

		[[nodiscard]] uint32_t GetVertexCount() const noexcept { return static_cast<uint32_t>( vertices_.size() ); }
		[[nodiscard]] uint32_t GetIndexCount() const noexcept { return static_cast<uint32_t>( indices_.size() ); }
		[[nodiscard]] uint32_t GetDrawCount() const noexcept { return static_cast<uint32_t>( indirectDraws_.size() ); }
		[[nodiscard]] uint32_t GetInstanceCount() const noexcept { return static_cast<uint32_t>( instances_.size() ); }

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

		struct GpuInstance
		{
			std::array<float, 16> model = {};
			DirectX::XMFLOAT4 color = {};
		};

		struct GeometryRange
		{
			int32_t firstVertex = 0;
			uint32_t firstIndex = 0;
			uint32_t indexCount = 0;
		};

		enum class GeometryTopology : uint8_t
		{
			Triangle,
			Line,
		};

		struct PushConstants
		{
			std::array<float, 16> viewProjection = {};
			uint32_t instanceBufferIndex = 0;
		};

		void Synchronize( const World& world );
		void RebuildRenderData( const World& world );
		void RebuildInstancesOnly( const World& world );
		void BuildBatch( const World& world, World::PrimitiveType primitive, bool wireframe );
		void BuildPrimitiveGeometry();
		void AppendCubeGeometry();
		void AppendSphereGeometry();
		void AppendArrowGeometry();
		void UploadStaticGeometry();
		void UploadInstanceBuffer();
		void UploadIndirectBuffer();
		void UploadDynamicBuffer( BufferHandle& buffer, uint64_t& capacity, const void* data, uint64_t elementCount, uint32_t stride, BufferType type, const char* debugName );
		void RetireBuffer( BufferHandle& buffer );
		void ReleaseBuffers();
		static uint64_t GrowCapacity( uint64_t requiredSize );
		static GpuInstance BuildGpuInstance( const World::Object& object );
		static PushConstants BuildPushConstants( RenderDevice& device, BufferHandle instanceBuffer, const Camera& camera );

		RenderDevice* device_ = nullptr;
		RenderPipelineState solidPipeline_;
		RenderPipelineState wireframePipeline_;
		RenderPipelineState linePipeline_;
		BufferHandle vertexBuffer_ = {};
		BufferHandle indexBuffer_ = {};
		BufferHandle instanceBuffer_ = {};
		BufferHandle indirectBuffer_ = {};
		std::vector<BufferHandle> retiredBuffers_;
		std::vector<Vertex> vertices_;
		std::vector<uint32_t> indices_;
		std::vector<GpuInstance> instances_;
		std::vector<IndirectDraw> indirectDraws_;
		std::vector<uint32_t> objectIndices_;
		static constexpr uint32_t ourPrimitiveCount = static_cast<uint32_t>( World::PrimitiveType::Count );
		std::array<GeometryRange, ourPrimitiveCount> geometries_ = {};
		std::array<GeometryTopology, ourPrimitiveCount> geometryTopologies_ = {};
		const World* synchronizedWorld_ = nullptr;
		uint64_t instanceBufferCapacity_ = 0;
		uint64_t indirectBufferCapacity_ = 0;
		uint64_t uploadedObjectRevision_ = std::numeric_limits<uint64_t>::max();
		uint64_t uploadedTransformRevision_ = std::numeric_limits<uint64_t>::max();
		uint32_t solidDrawCount_ = 0;
		uint32_t wireframeDrawCount_ = 0;
	};
}
