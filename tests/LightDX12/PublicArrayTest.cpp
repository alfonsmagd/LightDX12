#include "TestTemplate.hpp"

namespace lightd3d12::tests
{
	void TestPublicArrayProperties()
	{
		ShaderStageSource shader{};
		RenderPipelineDesc pipeline{};
		ContextDesc context{};

		Require( shader.includeDirectories.size() == ourMaxShaderIncludeDirectories,
			"Shader include array capacity differs from the public constant." );
		Require( pipeline.inputElements.size() == ourMaxVertexInputElements,
			"Vertex input array capacity differs from the public constant." );
		Require( context.bindlessCapacity == ourMaxBindlessDescriptors,
			"Default bindless capacity differs from the fixed array limit." );
		Require( context.rtvCapacity == ourMaxRtvDescriptors,
			"Default RTV capacity differs from the fixed array limit." );
		Require( context.dsvCapacity == ourMaxDsvDescriptors,
			"Default DSV capacity differs from the fixed array limit." );
		Require( sizeof( Handle<TestObject> ) == sizeof( uint64_t ),
			"LightD3D12 handles are not 64-bit values." );
	}
}
