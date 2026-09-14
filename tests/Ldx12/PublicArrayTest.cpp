#include "TestTemplate.hpp"

namespace ldx12::tests
{
	void TestPublicArrayProperties()
	{
		ShaderStageSource shader{};
		RenderPipelineDesc pipeline{};
		ContextDesc context{};

		Require( shader.includeDirectories.size() == ourMaxShaderIncludeDirectories, "Shader include array capacity differs from the public constant." );
		Require( pipeline.inputElements.size() == ourMaxVertexInputElements, "Vertex input array capacity differs from the public constant." );
		Require( pipeline.color[ 0 ].format == DXGI_FORMAT_R8G8B8A8_UNORM, "The default pipeline color attachment format is not RGBA8 UNORM." );
		Require( context.bindlessCapacity == ourMaxBindlessDescriptors, "Default bindless capacity differs from the fixed array limit." );
		Require( context.rtvCapacity == ourMaxRtvDescriptors, "Default RTV capacity differs from the fixed array limit." );
		Require( context.dsvCapacity == ourMaxDsvDescriptors, "Default DSV capacity differs from the fixed array limit." );
		Require( sizeof( Handle<TestObject> ) == sizeof( uint64_t ), "Ldx12 handles are not 64-bit values." );
	}
}
