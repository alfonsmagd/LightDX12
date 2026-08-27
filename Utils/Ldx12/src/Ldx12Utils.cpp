#include "Ldx12Utils/Ldx12Utils.hpp"

#include <algorithm>
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
		constexpr char ourWorldVertexShader[] = R"(
struct WorldTransform
{
    row_major float4x4 model;
};

cbuffer PushConstants : register(b0)
{
    row_major float4x4 viewProjection;
    uint transformBufferIndex;
};

struct BaseInstance
{
    uint index;
};

ConstantBuffer<BaseInstance> baseInstance : register(b1); // Written by ExecuteIndirect before each draw.

struct VSInput
{
    float3 position : POSITION;
};

struct VSOutput
{
    float4 position : SV_Position;
    float3 color : COLOR;
};

VSOutput main(VSInput input)
{
    StructuredBuffer<WorldTransform> transforms = ResourceDescriptorHeap[transformBufferIndex];
    const WorldTransform transform = transforms[baseInstance.index];
    const float4 worldPosition = mul(float4(input.position, 1.0), transform.model);

    VSOutput output;
    output.position = mul(worldPosition, viewProjection);
    output.color = baseInstance.index % 2 == 0
        ? float3(0.20, 0.65, 1.00)
        : float3(1.00, 0.45, 0.20);
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

		void ValidateTransform( const Transform& transform )
		{
			constexpr float minimumScale = 0.00001f;
			if( std::abs( transform.scale.x ) < minimumScale ||
				std::abs( transform.scale.y ) < minimumScale ||
				std::abs( transform.scale.z ) < minimumScale )
			{
				throw std::invalid_argument( "World object scale components must be non-zero." );
			}
		}

		RenderPipelineState CreateWorldPipeline( RenderDevice& device, const RenderWorldDesc& worldDesc )
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
	}

	MeshHandle World::AddCube( const CubeDesc& desc )
	{
		if( desc.size.x <= 0.0f || desc.size.y <= 0.0f || desc.size.z <= 0.0f )
		{
			throw std::invalid_argument( "Cube dimensions must be greater than zero." );
		}
		ValidateTransform( desc.transform );

		Object object{};
		object.geometry = GeometryType::Cube;
		object.transform = desc.transform;
		object.cubeSize = desc.size;
		return AddObject( std::move( object ) );
	}

	MeshHandle World::AddSphere( const SphereDesc& desc )
	{
		if( desc.radius <= 0.0f )
		{
			throw std::invalid_argument( "Sphere radius must be greater than zero." );
		}
		if( desc.longitudeSegments < 3 || desc.latitudeSegments < 2 )
		{
			throw std::invalid_argument( "Sphere requires at least 3 longitude and 2 latitude segments." );
		}
		ValidateTransform( desc.transform );

		Object object{};
		object.geometry = GeometryType::Sphere;
		object.transform = desc.transform;
		object.sphereRadius = desc.radius;
		object.longitudeSegments = desc.longitudeSegments;
		object.latitudeSegments = desc.latitudeSegments;
		return AddObject( std::move( object ) );
	}

	MeshHandle World::AddObject( Object&& object )
	{
		uint32_t index = 0;
		if( freeSlots_.empty() )
		{
			if( objects_.size() >= std::numeric_limits<uint32_t>::max() )
			{
				throw std::length_error( "World mesh capacity exhausted." );
			}
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
		meshCount_++;
		geometryRevision_++;
		transformRevision_++;
		return MeshHandle( index, slot.generation );
	}

	bool World::SetTransform( MeshHandle mesh, const Transform& transform )
	{
		Object* object = GetObject( mesh );
		if( object == nullptr )
		{
			return false;
		}

		ValidateTransform( transform );
		object->transform = transform;
		transformRevision_++;
		return true;
	}

	bool World::Destroy( MeshHandle mesh )
	{
		if( !Contains( mesh ) )
		{
			return false;
		}

		Slot& slot = objects_[ mesh.Index() ];
		slot.object = {};
		slot.occupied = false;
		IncrementGeneration( slot );
		freeSlots_.push_back( mesh.Index() );
		meshCount_--;
		geometryRevision_++;
		transformRevision_++;
		return true;
	}

	void World::Clear()
	{
		if( meshCount_ == 0 )
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

		meshCount_ = 0;
		geometryRevision_++;
		transformRevision_++;
	}

	bool World::Contains( MeshHandle mesh ) const noexcept
	{
		if( !mesh.Valid() || mesh.Index() >= objects_.size() )
		{
			return false;
		}

		const Slot& slot = objects_[ mesh.Index() ];
		return slot.occupied && slot.generation == mesh.Generation();
	}

	World::Object* World::GetObject( MeshHandle mesh ) noexcept
	{
		return Contains( mesh ) ? &objects_[ mesh.Index() ].object : nullptr;
	}

	void World::IncrementGeneration( Slot& slot ) noexcept
	{
		slot.generation++;
		if( slot.generation == 0 )
		{
			slot.generation = 1;
		}
	}

	RenderWorld::RenderWorld( RenderDevice& device, const RenderWorldDesc& desc ):
		device_( &device ),
		pipeline_( CreateWorldPipeline( device, desc ) )
	{
		static_assert( sizeof( Vertex ) == 12 );
		static_assert( sizeof( IndirectDraw ) == sizeof( uint32_t ) + sizeof( D3D12_DRAW_INDEXED_ARGUMENTS ) );
		static_assert( sizeof( GpuTransform ) == 64 );
		static_assert( sizeof( PushConstants ) == 68 );
	}

	RenderWorld::~RenderWorld()
	{
		ReleaseBuffers();
	}

	void RenderWorld::Render( ICommandBuffer& commands, const World& world, const Camera& camera )
	{
		Synchronize( world );
		if( indexCount_ == 0 )
		{
			return;
		}

		const PushConstants constants = BuildPushConstants( *device_, transformBuffer_, camera );
		ScopedCommandDebugGroup debugGroup( commands, "Ldx12 Utils World", 0xff4cc9f0u );
		commands.CmdBindRenderPipeline( pipeline_ );
		commands.CmdBindVertexBuffer( vertexBuffer_ );
		commands.CmdBindIndexBuffer( indexBuffer_ );
		commands.CmdPushConstants( &constants, sizeof( constants ) );
		commands.CmdDrawIndexedIndirect( indirectBuffer_, drawCount_ );
	}

	void RenderWorld::Synchronize( const World& world )
	{
		if( synchronizedWorld_ != &world || uploadedGeometryRevision_ != world.geometryRevision_ )
		{
			RebuildGeometry( world );
			UploadVertexBuffer();
			UploadIndexBuffer();
			UploadTransformBuffer();
			UploadIndirectBuffer();
			synchronizedWorld_ = &world;
			uploadedGeometryRevision_ = world.geometryRevision_;
			uploadedTransformRevision_ = world.transformRevision_;
			return;
		}

		if( uploadedTransformRevision_ != world.transformRevision_ )
		{
			RebuildTransforms( world );
			UploadTransformBuffer();
			uploadedTransformRevision_ = world.transformRevision_;
		}
	}

	void RenderWorld::RebuildGeometry( const World& world )
	{
		vertices_.clear();
		indices_.clear();
		transforms_.clear();
		indirectDraws_.clear();
		objectIndices_.assign( world.objects_.size(), std::numeric_limits<uint32_t>::max() );
		transforms_.reserve( world.meshCount_ );
		uint32_t baseInstance = 0;

		for( size_t slotIndex = 0; slotIndex < world.objects_.size(); ++slotIndex )
		{
			const World::Slot& slot = world.objects_[ slotIndex ];
			if( !slot.occupied )
			{
				continue;
			}

			const uint32_t objectIndex = baseInstance++;
			objectIndices_[ slotIndex ] = objectIndex;
			transforms_.push_back( BuildGpuTransform( slot.object.transform ) );
			const uint32_t firstIndex = static_cast<uint32_t>( indices_.size() );
			switch( slot.object.geometry )
			{
				case World::GeometryType::Cube:
					AppendCube( slot.object );
					break;
				case World::GeometryType::Sphere:
					AppendSphere( slot.object );
					break;
			}

			const uint64_t objectIndexCount = indices_.size() - firstIndex;
			if( objectIndexCount > std::numeric_limits<uint32_t>::max() )
			{
				throw std::length_error( "One world object exceeds the 32bit index count limit." );
			}
			IndirectDraw draw{};
			draw.baseInstance = objectIndex;
			draw.arguments.IndexCountPerInstance = static_cast<uint32_t>( objectIndexCount );
			draw.arguments.InstanceCount = 1;
			draw.arguments.StartIndexLocation = firstIndex;
			draw.arguments.BaseVertexLocation = 0;
			draw.arguments.StartInstanceLocation = 0;
			indirectDraws_.push_back( draw );
		}

		if( indices_.size() > std::numeric_limits<uint32_t>::max() )
		{
			throw std::length_error( "World index count exceeds the 32-bit draw limit." );
		}
		indexCount_ = static_cast<uint32_t>( indices_.size() );
		drawCount_ = static_cast<uint32_t>( indirectDraws_.size() );
	}

	void RenderWorld::RebuildTransforms( const World& world )
	{
		transforms_.assign( world.meshCount_, {} );
		for( size_t slotIndex = 0; slotIndex < world.objects_.size(); ++slotIndex )
		{
			const World::Slot& slot = world.objects_[ slotIndex ];
			if( !slot.occupied )
			{
				continue;
			}

			const uint32_t objectIndex = objectIndices_[ slotIndex ];
			if( objectIndex >= transforms_.size() )
			{
				throw std::runtime_error( "World transform mapping is out of date." );
			}
			transforms_[ objectIndex ] = BuildGpuTransform( slot.object.transform );
		}
	}

	void RenderWorld::AppendCube( const World::Object& object )
	{
		struct Face
		{
			std::array<XMFLOAT3, 4> corners;
		};

		const float halfX = object.cubeSize.x * 0.5f;
		const float halfY = object.cubeSize.y * 0.5f;
		const float halfZ = object.cubeSize.z * 0.5f;
		const std::array<Face, 6> faces = {
			Face{ { XMFLOAT3{ -halfX, -halfY, -halfZ }, XMFLOAT3{ -halfX, halfY, -halfZ }, XMFLOAT3{ halfX, halfY, -halfZ }, XMFLOAT3{ halfX, -halfY, -halfZ } } },
			Face{ { XMFLOAT3{ halfX, -halfY, halfZ }, XMFLOAT3{ halfX, halfY, halfZ }, XMFLOAT3{ -halfX, halfY, halfZ }, XMFLOAT3{ -halfX, -halfY, halfZ } } },
			Face{ { XMFLOAT3{ -halfX, -halfY, halfZ }, XMFLOAT3{ -halfX, halfY, halfZ }, XMFLOAT3{ -halfX, halfY, -halfZ }, XMFLOAT3{ -halfX, -halfY, -halfZ } } },
			Face{ { XMFLOAT3{ halfX, -halfY, -halfZ }, XMFLOAT3{ halfX, halfY, -halfZ }, XMFLOAT3{ halfX, halfY, halfZ }, XMFLOAT3{ halfX, -halfY, halfZ } } },
			Face{ { XMFLOAT3{ -halfX, halfY, -halfZ }, XMFLOAT3{ -halfX, halfY, halfZ }, XMFLOAT3{ halfX, halfY, halfZ }, XMFLOAT3{ halfX, halfY, -halfZ } } },
			Face{ { XMFLOAT3{ -halfX, -halfY, halfZ }, XMFLOAT3{ -halfX, -halfY, -halfZ }, XMFLOAT3{ halfX, -halfY, -halfZ }, XMFLOAT3{ halfX, -halfY, halfZ } } },
		};

		for( const Face& face : faces )
		{
			if( vertices_.size() > std::numeric_limits<uint32_t>::max() - 4u )
			{
				throw std::length_error( "World vertex count exceeds the 32-bit index limit." );
			}
			const uint32_t firstVertex = static_cast<uint32_t>( vertices_.size() );
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
		}
	}

	void RenderWorld::AppendSphere( const World::Object& object )
	{
		const uint32_t columns = object.longitudeSegments + 1u;
		const uint64_t addedVertexCount = static_cast<uint64_t>( columns ) * static_cast<uint64_t>( object.latitudeSegments + 1u );
		const uint64_t currentVertexCount = static_cast<uint64_t>( vertices_.size() );
		if( currentVertexCount > std::numeric_limits<uint32_t>::max() ||
			addedVertexCount > std::numeric_limits<uint32_t>::max() - currentVertexCount )
		{
			throw std::length_error( "World vertex count exceeds the 32-bit index limit." );
		}

		const uint32_t firstVertex = static_cast<uint32_t>( vertices_.size() );
		for( uint32_t latitude = 0; latitude <= object.latitudeSegments; ++latitude )
		{
			const float latitudeRatio = static_cast<float>( latitude ) / static_cast<float>( object.latitudeSegments );
			const float phi = latitudeRatio * std::numbers::pi_v<float>;
			const float y = std::cos( phi );
			const float ringRadius = std::sin( phi );

			for( uint32_t longitude = 0; longitude <= object.longitudeSegments; ++longitude )
			{
				const float longitudeRatio = static_cast<float>( longitude ) / static_cast<float>( object.longitudeSegments );
				const float theta = longitudeRatio * std::numbers::pi_v<float> * 2.0f;
				const XMFLOAT3 position = {
					ringRadius * std::cos( theta ) * object.sphereRadius,
					y * object.sphereRadius,
					ringRadius * std::sin( theta ) * object.sphereRadius };
				vertices_.push_back( { position } );
			}
		}

		for( uint32_t latitude = 0; latitude < object.latitudeSegments; ++latitude )
		{
			for( uint32_t longitude = 0; longitude < object.longitudeSegments; ++longitude )
			{
				const uint32_t topLeft = firstVertex + latitude * columns + longitude;
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

	void RenderWorld::UploadVertexBuffer()
	{
		const uint64_t requiredSize = static_cast<uint64_t>( vertices_.size() ) * sizeof( Vertex );
		if( requiredSize == 0 )
		{
			return;
		}

		if( !vertexBuffer_.Valid() || requiredSize > vertexBufferCapacity_ )
		{
			BufferDesc desc{};
			desc.debugName = "Ldx12 Utils World Vertices";
			desc.size = GrowCapacity( requiredSize );
			desc.stride = sizeof( Vertex );
			desc.type = BufferType::Vertex;
			const BufferHandle newBuffer = device_->CreateBuffer( desc );
			device_->WriteBuffer( newBuffer, 0, vertices_.data(), requiredSize );
			RetireBuffer( vertexBuffer_ );
			vertexBuffer_ = newBuffer;
			vertexBufferCapacity_ = desc.size;
			return;
		}

		device_->WriteBuffer( vertexBuffer_, 0, vertices_.data(), requiredSize );
	}

	void RenderWorld::UploadIndexBuffer()
	{
		const uint64_t requiredSize = static_cast<uint64_t>( indices_.size() ) * sizeof( uint32_t );
		if( requiredSize == 0 )
		{
			return;
		}

		if( !indexBuffer_.Valid() || requiredSize > indexBufferCapacity_ )
		{
			BufferDesc desc{};
			desc.debugName = "Ldx12 Utils World Indices";
			desc.size = GrowCapacity( requiredSize );
			desc.stride = sizeof( uint32_t );
			desc.type = BufferType::Index;
			const BufferHandle newBuffer = device_->CreateBuffer( desc );
			device_->WriteBuffer( newBuffer, 0, indices_.data(), requiredSize );
			RetireBuffer( indexBuffer_ );
			indexBuffer_ = newBuffer;
			indexBufferCapacity_ = desc.size;
			return;
		}

		device_->WriteBuffer( indexBuffer_, 0, indices_.data(), requiredSize );
	}

	void RenderWorld::UploadTransformBuffer()
	{
		const uint64_t requiredSize = static_cast<uint64_t>( transforms_.size() ) * sizeof( GpuTransform );
		if( requiredSize == 0 )
		{
			return;
		}

		if( !transformBuffer_.Valid() || requiredSize > transformBufferCapacity_ )
		{
			BufferDesc desc{};
			desc.debugName = "Ldx12 Utils World Transforms";
			desc.size = GrowCapacity( requiredSize );
			desc.stride = sizeof( GpuTransform );
			desc.type = BufferType::Structured;
			const BufferHandle newBuffer = device_->CreateBuffer( desc );
			device_->WriteBuffer( newBuffer, 0, transforms_.data(), requiredSize );
			RetireBuffer( transformBuffer_ );
			transformBuffer_ = newBuffer;
			transformBufferCapacity_ = desc.size;
			return;
		}

		device_->WriteBuffer( transformBuffer_, 0, transforms_.data(), requiredSize );
	}

	void RenderWorld::UploadIndirectBuffer()
	{
		const uint64_t requiredSize = static_cast<uint64_t>( indirectDraws_.size() ) * sizeof( IndirectDraw );
		if( requiredSize == 0 )
		{
			return;
		}

		if( !indirectBuffer_.Valid() || requiredSize > indirectBufferCapacity_ )
		{
			BufferDesc desc{};
			desc.debugName = "Ldx12 Utils World Indirect Draws";
			desc.size = GrowCapacity( requiredSize );
			desc.stride = sizeof( IndirectDraw );
			desc.type = BufferType::Indirect;

			const BufferHandle newBuffer = device_->CreateBuffer( desc );
			device_->WriteBuffer( newBuffer, 0, indirectDraws_.data(), requiredSize );
			RetireBuffer( indirectBuffer_ );
			indirectBuffer_ = newBuffer;
			indirectBufferCapacity_ = desc.size;
			return;
		}

		device_->WriteBuffer( indirectBuffer_, 0, indirectDraws_.data(), requiredSize );
	}

	void RenderWorld::RetireBuffer( BufferHandle& buffer )
	{
		if( buffer.Valid() )
		{
			retiredBuffers_.push_back( buffer );
			buffer = {};
		}
	}

	void RenderWorld::ReleaseBuffers()
	{
		if( device_ == nullptr )
		{
			return;
		}

		if( vertexBuffer_.Valid() || indexBuffer_.Valid() || transformBuffer_.Valid() || indirectBuffer_.Valid() || !retiredBuffers_.empty() )
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
		if( transformBuffer_.Valid() )
		{
			device_->Destroy( transformBuffer_ );
			transformBuffer_ = {};
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

	uint64_t RenderWorld::GrowCapacity( uint64_t requiredSize )
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

	RenderWorld::GpuTransform RenderWorld::BuildGpuTransform( const Transform& transform )
	{
		ValidateTransform( transform );
		const XMMATRIX scale = XMMatrixScaling( transform.scale.x, transform.scale.y, transform.scale.z );
		const XMMATRIX rotation = XMMatrixRotationRollPitchYaw( transform.rotation.x, transform.rotation.y, transform.rotation.z );
		const XMMATRIX translation = XMMatrixTranslation( transform.position.x, transform.position.y, transform.position.z );
		const XMMATRIX model = scale * rotation * translation;

		GpuTransform result{};
		StoreMatrix( result.model, model );
		return result;
	}

	RenderWorld::PushConstants RenderWorld::BuildPushConstants( RenderDevice& device, BufferHandle transformBuffer, const Camera& camera )
	{
		if( camera.aspectRatio <= 0.0f || camera.nearPlane <= 0.0f || camera.farPlane <= camera.nearPlane ||
			camera.verticalFieldOfViewRadians <= 0.0f || camera.verticalFieldOfViewRadians >= std::numbers::pi_v<float> )
		{
			throw std::invalid_argument( "World camera projection values are invalid." );
		}

		const XMVECTOR eye = XMVectorSet( camera.position.x, camera.position.y, camera.position.z, 1.0f );
		const XMVECTOR target = XMVectorSet( camera.target.x, camera.target.y, camera.target.z, 1.0f );
		const XMVECTOR up = XMVectorSet( camera.up.x, camera.up.y, camera.up.z, 0.0f );
		const XMMATRIX view = XMMatrixLookAtLH( eye, target, up );
		const XMMATRIX projection = XMMatrixPerspectiveFovLH(
			camera.verticalFieldOfViewRadians, camera.aspectRatio, camera.nearPlane, camera.farPlane );

		PushConstants constants{};
		StoreMatrix( constants.viewProjection, view * projection );
		constants.transformBufferIndex = device.GetBindlessIndex( transformBuffer );
		return constants;
	}
}
