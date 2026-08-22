#include "App/NodeGraph.hpp"

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace
{
	void Require( bool condition, const char* message )
	{
		if( !condition ) throw std::runtime_error( message );
	}
}

int main()
{
	try
	{
		App::NodeGraph graph;
		const App::NodeId three = graph.AddNode( App::NodeKind::FloatConstant, 3.0f );
		const App::NodeId two = graph.AddNode( App::NodeKind::FloatConstant, 2.0f );
		const App::NodeId add = graph.AddNode( App::NodeKind::Add );
		const App::NodeId subtract = graph.AddNode( App::NodeKind::Subtract );
		const App::NodeId multiply = graph.AddNode( App::NodeKind::Multiply );
		const App::NodeId compose = graph.AddNode( App::NodeKind::ComposeVector3 );
		const App::NodeId length = graph.AddNode( App::NodeKind::VectorLength );
		const auto connect = [&graph]( App::NodeId source, App::NodeId destination, size_t input )
		{
			return graph.Connect( graph.FindNode( source )->outputs[0].id, graph.FindNode( destination )->inputs[input].id );
		};
		Require( connect( three, add, 0 ) && connect( two, add, 1 ), "Cannot connect add node." );
		Require( connect( three, subtract, 0 ) && connect( two, subtract, 1 ), "Cannot connect subtract node." );
		Require( connect( add, multiply, 0 ) && connect( subtract, multiply, 1 ), "Cannot connect multiply node." );
		Require( connect( add, compose, 0 ) && connect( subtract, compose, 1 ) && connect( multiply, compose, 2 ), "Cannot connect Vector3 node." );
		Require( connect( compose, length, 0 ), "Cannot connect Vector3 length node." );
		const App::EvaluationResult vectorResult = graph.Evaluate( compose );
		Require( vectorResult.valid && std::holds_alternative<App::Float3>( vectorResult.value ), "Vector3 evaluation failed." );
		const App::Float3 vector = std::get<App::Float3>( vectorResult.value );
		Require( vector.x == 5.0f && vector.y == 1.0f && vector.z == 5.0f, "Vector3 evaluation result is incorrect." );
		const App::EvaluationResult lengthResult = graph.Evaluate( length );
		Require( lengthResult.valid && std::abs( std::get<float>( lengthResult.value ) - std::sqrt( 51.0f ) ) < 0.001f, "Vector length evaluation is incorrect." );
		std::string connectionError;
		Require( !graph.Connect( graph.FindNode( compose )->outputs[0].id, graph.FindNode( add )->inputs[0].id, &connectionError ), "Incompatible pin types were accepted." );
		const App::NodeId textureSource = graph.AddNode( App::NodeKind::TextureSource );
		const App::NodeId textureModifier = graph.AddNode( App::NodeKind::TextureCpuModifier );
		const App::NodeId texturePreview = graph.AddNode( App::NodeKind::TexturePreview );
		graph.FindNode( textureSource )->textureValue = { 77, 256, 128 };
		Require( connect( textureSource, textureModifier, 0 ), "Cannot connect Texture2D CPU modifier node." );
		Require( connect( textureModifier, texturePreview, 0 ), "Cannot connect Texture2D preview node." );
		const App::EvaluationResult textureResult = graph.Evaluate( texturePreview );
		Require( textureResult.valid && std::holds_alternative<App::TextureReference>( textureResult.value ), "Texture2D evaluation failed." );
		const App::TextureReference texture = std::get<App::TextureReference>( textureResult.value );
		Require( texture.resourceId == 77 && texture.width == 256 && texture.height == 128, "Texture2D evaluation result is incorrect." );

		std::cout << "LightDX12 NodeGraph tests passed.\n";
		return 0;
	}
	catch( const std::exception& exception )
	{
		std::cerr << exception.what() << '\n';
		return 1;
	}
}
