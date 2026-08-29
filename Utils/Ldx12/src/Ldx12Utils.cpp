#include "Ldx12Utils/Ldx12Utils.hpp"

#include <cmath>
#include <cstddef>
#include <cstring>
#include <numbers>
#include <stdexcept>
#include <utility>

using namespace DirectX;

namespace ldx12::utils
{
	namespace
	{
		constexpr uint32_t ourSphereLongitudeSegments = 24;
		constexpr uint32_t ourSphereLatitudeSegments = 16;
		constexpr float ourArrowHeadWidth = 0.10f;
		constexpr float ourArrowHeadStart = 0.80f;

		constexpr char ourWorldVertexShader[] = R"(
struct WorldInstance
{
    row_major float4x4 model;
    float4 color;
};

cbuffer PushConstants : register(b0)
{
    row_major float4x4 viewProjection;
    uint instanceBufferIndex;
};

struct BaseInstance
{
    uint index;
};

ConstantBuffer<BaseInstance> baseInstance : register(b1); // Written by ExecuteIndirect before each draw.

struct VSInput
{
    float3 position : POSITION;
    uint instanceId : SV_InstanceID;
};

struct VSOutput
{
    float4 position : SV_Position;
    float3 color : COLOR;
};

VSOutput main(VSInput input)
{
    StructuredBuffer<WorldInstance> instances = ResourceDescriptorHeap[instanceBufferIndex];
    const WorldInstance instance = instances[baseInstance.index + input.instanceId];
    const float4 worldPosition = mul(float4(input.position, 1.0), instance.model);

    VSOutput output;
    output.position = mul(worldPosition, viewProjection);
    output.color = instance.color.rgb;
    return output;
}
)";

		constexpr char ourWorldPixelShader[] = R"(
struct PSInput
{
    float4 position : SV_Position;
    float3 color : COLOR;
};

float4 main(PSInput input) : SV_Target0
{
    return float4(input.color, 1.0);
}
)";

		RenderPipelineState CreateWorldPipeline(
			RenderDevice& device,
			const DebugRendererDesc& worldDesc,
			D3D12_FILL_MODE fillMode,
			D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE,
			D3D_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST )
		{
			RenderPipelineDesc desc{};
			desc.vertexShader.source = ourWorldVertexShader;
			desc.vertexShader.entryPoint = "main";
			desc.vertexShader.profile = "vs_6_6";
			desc.vertexShader.sourceName = "Ldx12UtilsWorldVS";
			desc.fragmentShader.source = ourWorldPixelShader;
			desc.fragmentShader.entryPoint = "main";
			desc.fragmentShader.profile = "ps_6_6";
			desc.fragmentShader.sourceName = "Ldx12UtilsWorldPS";
			desc.color[ 0 ].format = worldDesc.colorFormat;
			desc.colorFormat = worldDesc.colorFormat;
			desc.depthFormat = worldDesc.depthFormat;
			desc.primitiveType = primitiveType;
			desc.topology = topology;
			desc.rasterizerState.FillMode = fillMode;
			desc.rasterizerState.CullMode = D3D12_CULL_MODE_NONE;
			desc.depthStencilState.DepthEnable = worldDesc.depthFormat != DXGI_FORMAT_UNKNOWN;
			desc.depthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
			desc.depthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
			desc.depthStencilState.StencilEnable = FALSE;

			desc.inputElements[ 0 ].semanticName = "POSITION";
			desc.inputElements[ 0 ].format = DXGI_FORMAT_R32G32B32_FLOAT;
			desc.inputElements[ 0 ].alignedByteOffset = 0;
			return device.CreateRenderPipeline( desc );
		}

		void StoreMatrix( std::array<float, 16>& destination, FXMMATRIX matrix ) noexcept
		{
			XMFLOAT4X4 storedMatrix{};
			XMStoreFloat4x4( &storedMatrix, matrix );
			std::memcpy( destination.data(), &storedMatrix, sizeof( storedMatrix ) );
		}

		void ValidateTransform( const Transform& transform )
		{
			if( transform.scale.x == 0.0f || transform.scale.y == 0.0f || transform.scale.z == 0.0f )
			{
				throw std::invalid_argument( "World transform scale components must not be zero." );
			}
		}
	}

	ObjectHandle World::AddCube( const CubeDesc& desc )
	{
		if( desc.size.x <= 0.0f || desc.size.y <= 0.0f || desc.size.z <= 0.0f )
		{
			throw std::invalid_argument( "Cube dimensions must be greater than zero." );
		}
		ValidateTransform( desc.transform );

		Object object{};
		object.primitive = PrimitiveType::Cube;
		object.transform = desc.transform;
		object.cubeSize = desc.size;
		object.color = desc.color;
		object.wireframe = desc.wireframe;
		return AddObject( std::move( object ) );
	}

	ObjectHandle World::AddSphere( const SphereDesc& desc )
	{
		if( desc.radius <= 0.0f )
		{
			throw std::invalid_argument( "Sphere radius must be greater than zero." );
		}
		ValidateTransform( desc.transform );

		Object object{};
		object.primitive = PrimitiveType::Sphere;
		object.transform = desc.transform;
		object.sphereRadius = desc.radius;
		object.color = desc.color;
		object.wireframe = desc.wireframe;
		return AddObject( std::move( object ) );
	}

	ObjectHandle World::AddArrow( const ArrowDesc& desc )
	{
		const XMVECTOR start = XMLoadFloat3( &desc.start );
		const XMVECTOR end = XMLoadFloat3( &desc.end );
		if( XMVectorGetX( XMVector3LengthSq( end - start ) ) == 0.0f )
		{
			throw std::invalid_argument( "Arrow start and end points must be different." );
		}

		Object object{};
		object.primitive = PrimitiveType::Arrow;
		object.arrowStart = desc.start;
		object.arrowEnd = desc.end;
		object.color = desc.color;
		return AddObject( std::move( object ) );
	}

	ObjectHandle World::AddObject( Object&& object )
	{
		uint32_t index = 0;
		if( freeSlots_.empty() )
		{
			index = static_cast<uint32_t>( objects_.size() );
			objects_.push_back( {} );
		}
		else
		{
			index = freeSlots_.back();
			freeSlots_.pop_back();
		}

		Slot& slot = objects_[ index ];
		slot.object = std::move( object );
		slot.occupied = true;
		objectCount_++;
		objectRevision_++;
		transformRevision_++;
		return ObjectHandle( index, slot.generation );
	}

	bool World::SetTransform( ObjectHandle objectHandle, const Transform& transform )
	{
		Object* object = GetObject( objectHandle );
		if( object == nullptr )
		{
			return false;
		}

		ValidateTransform( transform );
		object->transform = transform;
		transformRevision_++;
		return true;
	}

	bool World::Destroy( ObjectHandle object )
	{
		if( !Contains( object ) )
		{
			return false;
		}

		Slot& slot = objects_[ object.Index() ];
		slot.object = {};
		slot.occupied = false;
		IncrementGeneration( slot );
		freeSlots_.push_back( object.Index() );
		objectCount_--;
		objectRevision_++;
		transformRevision_++;
		return true;
	}

	void World::Clear()
	{
		if( objectCount_ == 0 )
		{
			return;
		}

		freeSlots_.clear();
		freeSlots_.reserve( objects_.size() );
		for( size_t index = 0; index < objects_.size(); ++index )
		{
			Slot& slot = objects_[ index ];
			if( slot.occupied )
			{
				slot.object = {};
				slot.occupied = false;
				IncrementGeneration( slot );
			}
			freeSlots_.push_back( static_cast<uint32_t>( index ) );
		}

		objectCount_ = 0;
		objectRevision_++;
		transformRevision_++;
	}

	bool World::Contains( ObjectHandle object ) const noexcept
	{
		if( !object.Valid() || object.Index() >= objects_.size() )
		{
			return false;
		}

		const Slot& slot = objects_[ object.Index() ];
		return slot.occupied && slot.generation == object.Generation();
	}

	World::Object* World::GetObject( ObjectHandle object ) noexcept
	{
		return Contains( object ) ? &objects_[ object.Index() ].object : nullptr;
	}

	void World::IncrementGeneration( Slot& slot ) noexcept
	{
		slot.generation++;
		if( slot.generation == 0 )
		{
			slot.generation = 1;
		}
	}

	DebugRenderer::DebugRenderer( RenderDevice& device, const DebugRendererDesc& desc ):
		device_( &device ),
		solidPipeline_( CreateWorldPipeline( device, desc, D3D12_FILL_MODE_SOLID ) ),
		wireframePipeline_( CreateWorldPipeline( device, desc, D3D12_FILL_MODE_WIREFRAME ) ),
		linePipeline_( CreateWorldPipeline(
			device,
			desc,
			D3D12_FILL_MODE_SOLID,
			D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE,
			D3D_PRIMITIVE_TOPOLOGY_LINELIST ) )
	{
		BuildPrimitiveGeometry();
		UploadStaticGeometry();
	}

	DebugRenderer::~DebugRenderer()
	{
		ReleaseBuffers();
	}

	void DebugRenderer::Render( ICommandBuffer& commands, const World& world, const Camera& camera )
	{
		Synchronize( world );
		if( indirectDraws_.empty() )
		{
			return;
		}

		const PushConstants constants = BuildPushConstants( *device_, instanceBuffer_, camera );
		ScopedCommandDebugGroup debugGroup( commands, "Ldx12 Utils World", 0xff4cc9f0u );
		commands.CmdBindVertexBuffer( vertexBuffer_ );
		commands.CmdBindIndexBuffer( indexBuffer_ );
		commands.CmdPushConstants( &constants, sizeof( constants ) );

		if( solidDrawCount_ > 0 )
		{
			commands.CmdBindRenderPipeline( solidPipeline_ );
			commands.CmdDrawIndexedIndirect( indirectBuffer_, solidDrawCount_ );
		}

		if( wireframeDrawCount_ > 0 )
		{
			commands.CmdBindRenderPipeline( wireframePipeline_ );
			commands.CmdDrawIndexedIndirect(
				indirectBuffer_,
				wireframeDrawCount_,
				static_cast<uint64_t>( solidDrawCount_ ) * sizeof( IndirectDraw ) );
		}

		const uint32_t lineDrawCount = static_cast<uint32_t>( indirectDraws_.size() ) -
			solidDrawCount_ - wireframeDrawCount_;
		if( lineDrawCount > 0 )
		{
			commands.CmdBindRenderPipeline( linePipeline_ );
			commands.CmdDrawIndexedIndirect(
				indirectBuffer_,
				lineDrawCount,
				static_cast<uint64_t>( solidDrawCount_ + wireframeDrawCount_ ) * sizeof( IndirectDraw ) );
		}
	}

	void DebugRenderer::Synchronize( const World& world )
	{
		if( synchronizedWorld_ != &world || uploadedObjectRevision_ != world.objectRevision_ )
		{
			RebuildRenderData( world );
			UploadInstanceBuffer();
			UploadIndirectBuffer();
			synchronizedWorld_ = &world;
			uploadedObjectRevision_ = world.objectRevision_;
			uploadedTransformRevision_ = world.transformRevision_;
			return;
		}

		if( uploadedTransformRevision_ != world.transformRevision_ )
		{
			RebuildInstancesOnly( world );
			UploadInstanceBuffer();
			uploadedTransformRevision_ = world.transformRevision_;
		}
	}

	void DebugRenderer::RebuildRenderData( const World& world )
	{
		instances_.clear();
		indirectDraws_.clear();
		objectIndices_.assign( world.objects_.size(), std::numeric_limits<uint32_t>::max() );
		instances_.reserve( world.objectCount_ );

		for( uint32_t primitiveIndex = 0; primitiveIndex < ourPrimitiveCount; ++primitiveIndex )
		{
			if( geometryTopologies_[ primitiveIndex ] != GeometryTopology::Triangle )
			{
				continue;
			}
			const World::PrimitiveType primitive = static_cast<World::PrimitiveType>( primitiveIndex );
			BuildBatch( world, primitive, false );
		}
		solidDrawCount_ = static_cast<uint32_t>( indirectDraws_.size() );

		for( uint32_t primitiveIndex = 0; primitiveIndex < ourPrimitiveCount; ++primitiveIndex )
		{
			if( geometryTopologies_[ primitiveIndex ] != GeometryTopology::Triangle )
			{
				continue;
			}
			const World::PrimitiveType primitive = static_cast<World::PrimitiveType>( primitiveIndex );
			BuildBatch( world, primitive, true );
		}
		wireframeDrawCount_ = static_cast<uint32_t>( indirectDraws_.size() ) - solidDrawCount_;

		for( uint32_t primitiveIndex = 0; primitiveIndex < ourPrimitiveCount; ++primitiveIndex )
		{
			if( geometryTopologies_[ primitiveIndex ] != GeometryTopology::Line )
			{
				continue;
			}
			const World::PrimitiveType primitive = static_cast<World::PrimitiveType>( primitiveIndex );
			BuildBatch( world, primitive, false );
		}
	}

	void DebugRenderer::RebuildInstancesOnly( const World& world )
	{
		for( size_t slotIndex = 0; slotIndex < world.objects_.size(); ++slotIndex )
		{
			const World::Slot& slot = world.objects_[ slotIndex ];
			if( slot.occupied )
			{
				const uint32_t instanceIndex = objectIndices_[ slotIndex ];
				instances_[ instanceIndex ] = BuildGpuInstance( slot.object );
			}
		}
	}

	void DebugRenderer::BuildBatch( const World& world, World::PrimitiveType primitive, bool wireframe )
	{
		const uint32_t primitiveIndex = static_cast<uint32_t>( primitive );
		const GeometryRange& geometry = geometries_[ primitiveIndex ];
		const uint32_t firstInstance = static_cast<uint32_t>( instances_.size() );
		for( size_t slotIndex = 0; slotIndex < world.objects_.size(); ++slotIndex )
		{
			const World::Slot& slot = world.objects_[ slotIndex ];
			if( !slot.occupied || slot.object.primitive != primitive || slot.object.wireframe != wireframe )
			{
				continue;
			}

			objectIndices_[ slotIndex ] = static_cast<uint32_t>( instances_.size() );
			instances_.push_back( BuildGpuInstance( slot.object ) );
		}

		const uint32_t instanceCount = static_cast<uint32_t>( instances_.size() ) - firstInstance;
		if( instanceCount == 0 )
		{
			return;
		}

		IndirectDraw draw{};
		draw.baseInstance = firstInstance;
		draw.arguments.IndexCountPerInstance = geometry.indexCount;
		draw.arguments.InstanceCount = instanceCount;
		draw.arguments.StartIndexLocation = geometry.firstIndex;
		draw.arguments.BaseVertexLocation = geometry.firstVertex;
		draw.arguments.StartInstanceLocation = 0;
		indirectDraws_.push_back( draw );
	}

	void DebugRenderer::BuildPrimitiveGeometry()
	{
		GeometryRange& cube = geometries_[ static_cast<uint32_t>( World::PrimitiveType::Cube ) ];
		cube.firstVertex = static_cast<int32_t>( vertices_.size() );
		cube.firstIndex = static_cast<uint32_t>( indices_.size() );
		AppendCubeGeometry();
		cube.indexCount = static_cast<uint32_t>( indices_.size() ) - cube.firstIndex;

		GeometryRange& sphere = geometries_[ static_cast<uint32_t>( World::PrimitiveType::Sphere ) ];
		sphere.firstVertex = static_cast<int32_t>( vertices_.size() );
		sphere.firstIndex = static_cast<uint32_t>( indices_.size() );
		AppendSphereGeometry();
		sphere.indexCount = static_cast<uint32_t>( indices_.size() ) - sphere.firstIndex;

		GeometryRange& arrow = geometries_[ static_cast<uint32_t>( World::PrimitiveType::Arrow ) ];
		geometryTopologies_[ static_cast<uint32_t>( World::PrimitiveType::Arrow ) ] = GeometryTopology::Line;
		arrow.firstVertex = static_cast<int32_t>( vertices_.size() );
		arrow.firstIndex = static_cast<uint32_t>( indices_.size() );
		AppendArrowGeometry();
		arrow.indexCount = static_cast<uint32_t>( indices_.size() ) - arrow.firstIndex;
	}

	void DebugRenderer::AppendCubeGeometry()
	{
		struct Face
		{
			std::array<XMFLOAT3, 4> corners;
		};

		constexpr float halfX = 0.5f;
		constexpr float halfY = 0.5f;
		constexpr float halfZ = 0.5f;
		const std::array<Face, 6> faces = {
			Face{ { XMFLOAT3{ -halfX, -halfY, -halfZ }, XMFLOAT3{ -halfX, halfY, -halfZ }, XMFLOAT3{ halfX, halfY, -halfZ }, XMFLOAT3{ halfX, -halfY, -halfZ } } },
			Face{ { XMFLOAT3{ halfX, -halfY, halfZ }, XMFLOAT3{ halfX, halfY, halfZ }, XMFLOAT3{ -halfX, halfY, halfZ }, XMFLOAT3{ -halfX, -halfY, halfZ } } },
			Face{ { XMFLOAT3{ -halfX, -halfY, halfZ }, XMFLOAT3{ -halfX, halfY, halfZ }, XMFLOAT3{ -halfX, halfY, -halfZ }, XMFLOAT3{ -halfX, -halfY, -halfZ } } },
			Face{ { XMFLOAT3{ halfX, -halfY, -halfZ }, XMFLOAT3{ halfX, halfY, -halfZ }, XMFLOAT3{ halfX, halfY, halfZ }, XMFLOAT3{ halfX, -halfY, halfZ } } },
			Face{ { XMFLOAT3{ -halfX, halfY, -halfZ }, XMFLOAT3{ -halfX, halfY, halfZ }, XMFLOAT3{ halfX, halfY, halfZ }, XMFLOAT3{ halfX, halfY, -halfZ } } },
			Face{ { XMFLOAT3{ -halfX, -halfY, halfZ }, XMFLOAT3{ -halfX, -halfY, -halfZ }, XMFLOAT3{ halfX, -halfY, -halfZ }, XMFLOAT3{ halfX, -halfY, halfZ } } },
		};

		uint32_t faceIndex = 0;
		for( const Face& face : faces )
		{
			const uint32_t firstVertex = faceIndex * 4u;
			for( const XMFLOAT3& corner : face.corners )
			{
				vertices_.push_back( { corner } );
			}
			indices_.push_back( firstVertex );
			indices_.push_back( firstVertex + 1u );
			indices_.push_back( firstVertex + 2u );
			indices_.push_back( firstVertex );
			indices_.push_back( firstVertex + 2u );
			indices_.push_back( firstVertex + 3u );
			faceIndex++;
		}
	}

	void DebugRenderer::AppendSphereGeometry()
	{
		const uint32_t columns = ourSphereLongitudeSegments + 1u;
		for( uint32_t latitude = 0; latitude <= ourSphereLatitudeSegments; ++latitude )
		{
			const float latitudeRatio = static_cast<float>( latitude ) / static_cast<float>( ourSphereLatitudeSegments );
			const float phi = latitudeRatio * std::numbers::pi_v<float>;
			const float y = std::cos( phi );
			const float ringRadius = std::sin( phi );

			for( uint32_t longitude = 0; longitude <= ourSphereLongitudeSegments; ++longitude )
			{
				const float longitudeRatio = static_cast<float>( longitude ) / static_cast<float>( ourSphereLongitudeSegments );
				const float theta = longitudeRatio * std::numbers::pi_v<float> * 2.0f;
				const XMFLOAT3 position = {
					ringRadius * std::cos( theta ),
					y,
					ringRadius * std::sin( theta ) };
				vertices_.push_back( { position } );
			}
		}

		for( uint32_t latitude = 0; latitude < ourSphereLatitudeSegments; ++latitude )
		{
			for( uint32_t longitude = 0; longitude < ourSphereLongitudeSegments; ++longitude )
			{
				const uint32_t topLeft = latitude * columns + longitude;
				const uint32_t bottomLeft = topLeft + columns;
				indices_.push_back( topLeft );
				indices_.push_back( bottomLeft );
				indices_.push_back( topLeft + 1u );
				indices_.push_back( topLeft + 1u );
				indices_.push_back( bottomLeft );
				indices_.push_back( bottomLeft + 1u );
			}
		}
	}

	void DebugRenderer::AppendArrowGeometry()
	{
		vertices_.push_back( { { 0.0f, 0.0f, 0.0f } } );
		vertices_.push_back( { { 0.0f, 0.0f, 1.0f } } );
		vertices_.push_back( { { 0.0f, ourArrowHeadWidth, ourArrowHeadStart } } );
		vertices_.push_back( { { 0.0f, -ourArrowHeadWidth, ourArrowHeadStart } } );

		indices_.push_back( 0u );
		indices_.push_back( 1u );
		indices_.push_back( 1u );
		indices_.push_back( 2u );
		indices_.push_back( 1u );
		indices_.push_back( 3u );
	}

	void DebugRenderer::UploadStaticGeometry()
	{
		BufferDesc vertexDesc{};
		vertexDesc.debugName = "Ldx12 Utils World Vertices";
		vertexDesc.size = static_cast<uint64_t>( vertices_.size() ) * sizeof( Vertex );
		vertexDesc.stride = sizeof( Vertex );
		vertexDesc.type = BufferType::Vertex;
		vertexDesc.initialData = vertices_.data();
		vertexBuffer_ = device_->CreateBuffer( vertexDesc );

		BufferDesc indexDesc{};
		indexDesc.debugName = "Ldx12 Utils World Indices";
		indexDesc.size = static_cast<uint64_t>( indices_.size() ) * sizeof( uint32_t );
		indexDesc.stride = sizeof( uint32_t );
		indexDesc.type = BufferType::Index;
		indexDesc.initialData = indices_.data();
		indexBuffer_ = device_->CreateBuffer( indexDesc );
	}

	void DebugRenderer::UploadInstanceBuffer()
	{
		UploadDynamicBuffer(
			instanceBuffer_,
			instanceBufferCapacity_,
			instances_.data(),
			static_cast<uint64_t>( instances_.size() ),
			sizeof( GpuInstance ),
			BufferType::Structured,
			"Ldx12 Utils World Instances" );
	}

	void DebugRenderer::UploadIndirectBuffer()
	{
		UploadDynamicBuffer(
			indirectBuffer_,
			indirectBufferCapacity_,
			indirectDraws_.data(),
			static_cast<uint64_t>( indirectDraws_.size() ),
			sizeof( IndirectDraw ),
			BufferType::Indirect,
			"Ldx12 Utils World Indirect Draws" );
	}

	void DebugRenderer::UploadDynamicBuffer(
		BufferHandle& buffer,
		uint64_t& capacity,
		const void* data,
		uint64_t elementCount,
		uint32_t stride,
		BufferType type,
		const char* debugName )
	{
		if( elementCount == 0 )
		{
			return;
		}

		const uint64_t requiredSize = elementCount * stride;
		if( !buffer.Valid() || requiredSize > capacity )
		{
			BufferDesc desc{};
			desc.debugName = debugName;
			desc.size = GrowCapacity( elementCount ) * stride;
			desc.stride = stride;
			desc.type = type;
			const BufferHandle newBuffer = device_->CreateBuffer( desc );
			device_->WriteBuffer( newBuffer, 0, data, requiredSize );
			RetireBuffer( buffer );
			buffer = newBuffer;
			capacity = desc.size;
			return;
		}

		device_->WriteBuffer( buffer, 0, data, requiredSize );
	}

	void DebugRenderer::RetireBuffer( BufferHandle& buffer )
	{
		if( buffer.Valid() )
		{
			retiredBuffers_.push_back( buffer );
			buffer = {};
		}
	}

	void DebugRenderer::ReleaseBuffers()
	{
		if( device_ == nullptr )
		{
			return;
		}

		if( vertexBuffer_.Valid() || indexBuffer_.Valid() || instanceBuffer_.Valid() || indirectBuffer_.Valid() || !retiredBuffers_.empty() )
		{
			device_->WaitIdle();
		}
		if( vertexBuffer_.Valid() )
		{
			device_->Destroy( vertexBuffer_ );
			vertexBuffer_ = {};
		}
		if( indexBuffer_.Valid() )
		{
			device_->Destroy( indexBuffer_ );
			indexBuffer_ = {};
		}
		if( instanceBuffer_.Valid() )
		{
			device_->Destroy( instanceBuffer_ );
			instanceBuffer_ = {};
		}
		if( indirectBuffer_.Valid() )
		{
			device_->Destroy( indirectBuffer_ );
			indirectBuffer_ = {};
		}
		for( BufferHandle buffer : retiredBuffers_ )
		{
			device_->Destroy( buffer );
		}
		retiredBuffers_.clear();
	}

	uint64_t DebugRenderer::GrowCapacity( uint64_t requiredSize )
	{
		uint64_t capacity = 1;
		while( capacity < requiredSize )
		{
			if( capacity > std::numeric_limits<uint64_t>::max() / 2u )
			{
				return requiredSize;
			}
			capacity *= 2u;
		}
		return capacity;
	}

	DebugRenderer::GpuInstance DebugRenderer::BuildGpuInstance( const World::Object& object )
	{
		if( object.primitive == World::PrimitiveType::Arrow )
		{
			const XMVECTOR start = XMLoadFloat3( &object.arrowStart );
			const XMVECTOR end = XMLoadFloat3( &object.arrowEnd );
			const XMVECTOR direction = end - start;
			const float length = XMVectorGetX( XMVector3Length( direction ) );
			const XMVECTOR forward = XMVectorScale( direction, 1.0f / length );
			const XMVECTOR referenceUp = std::abs( XMVectorGetY( forward ) ) > 0.999f
				? XMVectorSet( 1.0f, 0.0f, 0.0f, 0.0f )
				: XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );
			const XMVECTOR unitRight = XMVector3Normalize( XMVector3Cross( referenceUp, forward ) );
			const XMVECTOR unitUp = XMVector3Cross( forward, unitRight );
			const XMMATRIX arrowModel(
				XMVectorScale( unitRight, length ),
				XMVectorScale( unitUp, length ),
				XMVectorScale( forward, length ),
				XMVectorSet( object.arrowStart.x, object.arrowStart.y, object.arrowStart.z, 1.0f ) );

			const XMMATRIX scale = XMMatrixScaling(
				object.transform.scale.x,
				object.transform.scale.y,
				object.transform.scale.z );
			const XMMATRIX rotation = XMMatrixRotationRollPitchYaw(
				object.transform.rotation.x,
				object.transform.rotation.y,
				object.transform.rotation.z );
			const XMMATRIX translation = XMMatrixTranslation(
				object.transform.position.x,
				object.transform.position.y,
				object.transform.position.z );

			GpuInstance result{};
			StoreMatrix( result.model, arrowModel * scale * rotation * translation );
			result.color = object.color;
			return result;
		}

		XMFLOAT3 geometryScale = object.cubeSize;
		if( object.primitive == World::PrimitiveType::Sphere )
		{
			geometryScale = { object.sphereRadius, object.sphereRadius, object.sphereRadius };
		}

		const XMMATRIX scale = XMMatrixScaling(
			object.transform.scale.x * geometryScale.x,
			object.transform.scale.y * geometryScale.y,
			object.transform.scale.z * geometryScale.z );
		const XMMATRIX rotation = XMMatrixRotationRollPitchYaw(
			object.transform.rotation.x,
			object.transform.rotation.y,
			object.transform.rotation.z );
		const XMMATRIX translation = XMMatrixTranslation(
			object.transform.position.x,
			object.transform.position.y,
			object.transform.position.z );
		const XMMATRIX model = scale * rotation * translation;

		GpuInstance result{};
		StoreMatrix( result.model, model );
		result.color = object.color;
		return result;
	}

	DebugRenderer::PushConstants DebugRenderer::BuildPushConstants( RenderDevice& device, BufferHandle instanceBuffer, const Camera& camera )
	{
		if( camera.aspectRatio <= 0.0f ||
			camera.nearPlane <= 0.0f ||
			camera.farPlane <= camera.nearPlane ||
			camera.verticalFieldOfViewRadians <= 0.0f ||
			camera.verticalFieldOfViewRadians >= std::numbers::pi_v<float> )
		{
			throw std::invalid_argument( "Camera projection values are invalid." );
		}

		const XMVECTOR eye = XMVectorSet( camera.position.x, camera.position.y, camera.position.z, 1.0f );
		const XMVECTOR target = XMVectorSet( camera.target.x, camera.target.y, camera.target.z, 1.0f );
		const XMVECTOR up = XMVectorSet( camera.up.x, camera.up.y, camera.up.z, 0.0f );
		const XMMATRIX view = XMMatrixLookAtLH( eye, target, up );
		const XMMATRIX projection = XMMatrixPerspectiveFovLH(
			camera.verticalFieldOfViewRadians, camera.aspectRatio, camera.nearPlane, camera.farPlane );

		PushConstants constants{};
		StoreMatrix( constants.viewProjection, view * projection );
		constants.instanceBufferIndex = device.GetBindlessIndex( instanceBuffer );
		return constants;
	}
}
