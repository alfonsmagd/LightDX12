#include "Ldx12Utils/Geometry.hpp"

#include <array>
#include <cmath>
#include <vector>

namespace ldx12::utils
{
	namespace
	{
		static constexpr std::array<GeometryVertex, 4> ourQuadVertices = { GeometryVertex{ { -1.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 1.0f } },
			GeometryVertex{ { -1.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f } },
			GeometryVertex{ { 1.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f } },
			GeometryVertex{ { 1.0f, -1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 1.0f } } };

		static constexpr std::array<uint32_t, 6> ourQuadIndices = { 0, 1, 2, 0, 2, 3 };

		static constexpr std::array<GeometryVertex, 24> ourCubeVertices = { GeometryVertex{ { -1.0f, -1.0f, -1.0f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 1.0f } },
			GeometryVertex{ { -1.0f, 1.0f, -1.0f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f } },
			GeometryVertex{ { 1.0f, 1.0f, -1.0f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f } },
			GeometryVertex{ { 1.0f, -1.0f, -1.0f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 1.0f } },
			GeometryVertex{ { 1.0f, -1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 1.0f } },
			GeometryVertex{ { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f } },
			GeometryVertex{ { -1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f } },
			GeometryVertex{ { -1.0f, -1.0f, 1.0f }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 1.0f } },
			GeometryVertex{ { -1.0f, -1.0f, 1.0f }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f } },
			GeometryVertex{ { -1.0f, 1.0f, 1.0f }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f } },
			GeometryVertex{ { -1.0f, 1.0f, -1.0f }, { -1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f } },
			GeometryVertex{ { -1.0f, -1.0f, -1.0f }, { -1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f } },
			GeometryVertex{ { 1.0f, -1.0f, -1.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f } },
			GeometryVertex{ { 1.0f, 1.0f, -1.0f }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f } },
			GeometryVertex{ { 1.0f, 1.0f, 1.0f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 0.0f } },
			GeometryVertex{ { 1.0f, -1.0f, 1.0f }, { 1.0f, 0.0f, 0.0f }, { 1.0f, 1.0f } },
			GeometryVertex{ { -1.0f, 1.0f, -1.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 1.0f } },
			GeometryVertex{ { -1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f } },
			GeometryVertex{ { 1.0f, 1.0f, 1.0f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 0.0f } },
			GeometryVertex{ { 1.0f, 1.0f, -1.0f }, { 0.0f, 1.0f, 0.0f }, { 1.0f, 1.0f } },
			GeometryVertex{ { -1.0f, -1.0f, 1.0f }, { 0.0f, -1.0f, 0.0f }, { 0.0f, 1.0f } },
			GeometryVertex{ { -1.0f, -1.0f, -1.0f }, { 0.0f, -1.0f, 0.0f }, { 0.0f, 0.0f } },
			GeometryVertex{ { 1.0f, -1.0f, -1.0f }, { 0.0f, -1.0f, 0.0f }, { 1.0f, 0.0f } },
			GeometryVertex{ { 1.0f, -1.0f, 1.0f }, { 0.0f, -1.0f, 0.0f }, { 1.0f, 1.0f } } };

		static constexpr std::array<uint32_t, 36> ourCubeIndices =
			{ 0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7, 8, 9, 10, 8, 10, 11, 12, 13, 14, 12, 14, 15, 16, 17, 18, 16, 18, 19, 20, 21, 22, 20, 22, 23 };

		static GeometryBuffers CreateGeometryBuffers( RenderDevice& device,
			const GeometryVertex* vertices,
			uint64_t vertexCount,
			const uint32_t* indices,
			uint64_t indexCount,
			const char* vertexName,
			const char* indexName )
		{
			BufferDesc vertexDesc{};
			vertexDesc.debugName = vertexName;
			vertexDesc.size = vertexCount * sizeof( GeometryVertex );
			vertexDesc.stride = sizeof( GeometryVertex );
			vertexDesc.type = BufferType::Vertex;
			vertexDesc.initialData = vertices;

			GeometryBuffers geometry{};
			geometry.vertexBuffer = device.CreateBuffer( vertexDesc );

			BufferDesc indexDesc{};
			indexDesc.debugName = indexName;
			indexDesc.size = indexCount * sizeof( uint32_t );
			indexDesc.stride = sizeof( uint32_t );
			indexDesc.type = BufferType::Index;
			indexDesc.initialData = indices;
			geometry.indexBuffer = device.CreateBuffer( indexDesc );
			geometry.indexCount = static_cast<uint32_t>( indexCount );
			return geometry;
		}
	}

	GeometryBuffers CreateQuad( RenderDevice& device )
	{
		return CreateGeometryBuffers( device,
			ourQuadVertices.data(),
			ourQuadVertices.size(),
			ourQuadIndices.data(),
			ourQuadIndices.size(),
			"Quad vertices",
			"Quad indices" );
	}

	GeometryBuffers CreateCube( RenderDevice& device )
	{
		return CreateGeometryBuffers( device,
			ourCubeVertices.data(),
			ourCubeVertices.size(),
			ourCubeIndices.data(),
			ourCubeIndices.size(),
			"Cube vertices",
			"Cube indices" );
	}

	GeometryBuffers CreateSphere( RenderDevice& device, uint32_t rings, uint32_t segments )
	{
		std::vector<GeometryVertex> vertices;
		std::vector<uint32_t> indices;
		vertices.reserve( static_cast<size_t>( rings + 1u ) * ( segments + 1u ) );
		indices.reserve( static_cast<size_t>( rings ) * segments * 6u );

		for( uint32_t ring = 0; ring <= rings; ++ring )
		{
			const float latitude = DirectX::XM_PI * static_cast<float>( ring ) / static_cast<float>( rings );
			const float ringRadius = std::sin( latitude );
			const float y = std::cos( latitude );
			for( uint32_t segment = 0; segment <= segments; ++segment )
			{
				const float longitude = DirectX::XM_2PI * static_cast<float>( segment ) / static_cast<float>( segments );
				const DirectX::XMFLOAT3 normal = { ringRadius * std::cos( longitude ), y, ringRadius * std::sin( longitude ) };
				const DirectX::XMFLOAT2 texCoord = { static_cast<float>( segment ) / static_cast<float>( segments ),
					static_cast<float>( ring ) / static_cast<float>( rings ) };
				vertices.push_back( { normal, normal, texCoord } );
			}
		}

		for( uint32_t ring = 0; ring < rings; ++ring )
		{
			for( uint32_t segment = 0; segment < segments; ++segment )
			{
				const uint32_t current = ring * ( segments + 1u ) + segment;
				const uint32_t nextRing = current + segments + 1u;
				indices.push_back( current );
				indices.push_back( nextRing );
				indices.push_back( current + 1u );
				indices.push_back( current + 1u );
				indices.push_back( nextRing );
				indices.push_back( nextRing + 1u );
			}
		}

		return CreateGeometryBuffers( device, vertices.data(), vertices.size(), indices.data(), indices.size(), "Sphere vertices", "Sphere indices" );
	}

	void DestroyGeometry( RenderDevice& device, GeometryBuffers& geometry )
	{
		device.Destroy( geometry.indexBuffer );
		device.Destroy( geometry.vertexBuffer );
		geometry = {};
	}
}
