#include "App/NodeGraph.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <unordered_set>

namespace App
{
	namespace
	{
		EvaluationResult Error( std::string message )
		{
			return { false, 0.0f, std::move( message ) };
		}

		EvaluationResult FloatResult( float value )
		{
			return { true, value, {} };
		}

		EvaluationResult VectorResult( Float3 value )
		{
			return { true, value, {} };
		}

		EvaluationResult TextureResult( TextureReference value )
		{
			return { true, value, {} };
		}
	}

	NodeId NodeGraph::AddNode( NodeKind kind, float initialValue )
	{
		GraphNode node;
		node.id = nextNodeId_++;
		node.kind = kind;
		node.title = NodeKindName( kind );
		node.constantValue = initialValue;
		switch( kind )
		{
		case NodeKind::FloatConstant:
			node.outputs.push_back( MakePin( node.id, "Valor", NodeValueType::Float, false ) );
			break;
		case NodeKind::Add:
		case NodeKind::Subtract:
		case NodeKind::Multiply:
			node.inputs.push_back( MakePin( node.id, "A", NodeValueType::Float, true ) );
			node.inputs.push_back( MakePin( node.id, "B", NodeValueType::Float, true ) );
			node.outputs.push_back( MakePin( node.id, "Resultado", NodeValueType::Float, false ) );
			break;
		case NodeKind::ComposeVector3:
			node.inputs.push_back( MakePin( node.id, "X", NodeValueType::Float, true ) );
			node.inputs.push_back( MakePin( node.id, "Y", NodeValueType::Float, true ) );
			node.inputs.push_back( MakePin( node.id, "Z", NodeValueType::Float, true ) );
			node.outputs.push_back( MakePin( node.id, "Vector", NodeValueType::Vector3, false ) );
			break;
		case NodeKind::VectorLength:
			node.inputs.push_back( MakePin( node.id, "Vector", NodeValueType::Vector3, true ) );
			node.outputs.push_back( MakePin( node.id, "Longitud", NodeValueType::Float, false ) );
			break;
		case NodeKind::TextureSource:
			node.outputs.push_back( MakePin( node.id, "Textura", NodeValueType::Texture2D, false ) );
			break;
		case NodeKind::TextureCpuModifier:
			node.inputs.push_back( MakePin( node.id, "Original", NodeValueType::Texture2D, true ) );
			node.outputs.push_back( MakePin( node.id, "Procesada", NodeValueType::Texture2D, false ) );
			break;
		case NodeKind::TexturePreview:
			node.inputs.push_back( MakePin( node.id, "Textura", NodeValueType::Texture2D, true ) );
			node.outputs.push_back( MakePin( node.id, "Textura", NodeValueType::Texture2D, false ) );
			break;
		}
		nodes_.push_back( std::move( node ) );
		return nodes_.back().id;
	}

	bool NodeGraph::RemoveNode( NodeId nodeId )
	{
		const auto node = std::find_if( nodes_.begin(), nodes_.end(), [ nodeId ]( const GraphNode& value ) { return value.id == nodeId; } );
		if( node == nodes_.end() )
			return false;
		std::unordered_set<PinId> pins;
		for( const NodePin& pin : node->inputs )
			pins.insert( pin.id );
		for( const NodePin& pin : node->outputs )
			pins.insert( pin.id );
		links_.erase( std::remove_if( links_.begin(),
						  links_.end(),
						  [ &pins ]( const GraphLink& link ) { return pins.contains( link.inputPin ) || pins.contains( link.outputPin ); } ),
			links_.end() );
		nodes_.erase( node );
		return true;
	}

	bool NodeGraph::Connect( PinId firstPin, PinId secondPin, std::string* error )
	{
		const NodePin* first = FindPin( firstPin );
		const NodePin* second = FindPin( secondPin );
		if( !first || !second )
		{
			if( error )
				*error = "El pin no existe.";
			return false;
		}
		const NodePin* output = first->input ? second : first;
		const NodePin* input = first->input ? first : second;
		if( output->input || !input->input )
		{
			if( error )
				*error = "Conecta una salida con una entrada.";
			return false;
		}
		if( output->type != input->type )
		{
			if( error )
				*error = std::string( "Tipos incompatibles: " ) + NodeValueTypeName( output->type ) + " -> " + NodeValueTypeName( input->type );
			return false;
		}
		if( output->nodeId == input->nodeId || WouldCreateCycle( output->nodeId, input->nodeId ) )
		{
			if( error )
				*error = "La conexion crearia un ciclo.";
			return false;
		}

		links_.erase( std::remove_if( links_.begin(), links_.end(), [ input ]( const GraphLink& link ) { return link.inputPin == input->id; } ), links_.end() );
		links_.push_back( { nextLinkId_++, output->id, input->id } );
		if( error )
			error->clear();
		return true;
	}

	bool NodeGraph::RemoveLink( LinkId linkId )
	{
		const auto link = std::find_if( links_.begin(), links_.end(), [ linkId ]( const GraphLink& value ) { return value.id == linkId; } );
		if( link == links_.end() )
			return false;
		links_.erase( link );
		return true;
	}

	void NodeGraph::Clear()
	{
		nodes_.clear();
		links_.clear();
	}

	GraphNode* NodeGraph::FindNode( NodeId nodeId )
	{
		const auto found = std::find_if( nodes_.begin(), nodes_.end(), [ nodeId ]( const GraphNode& node ) { return node.id == nodeId; } );
		return found == nodes_.end() ? nullptr : &*found;
	}

	const GraphNode* NodeGraph::FindNode( NodeId nodeId ) const
	{
		const auto found = std::find_if( nodes_.begin(), nodes_.end(), [ nodeId ]( const GraphNode& node ) { return node.id == nodeId; } );
		return found == nodes_.end() ? nullptr : &*found;
	}

	const NodePin* NodeGraph::FindPin( PinId pinId ) const
	{
		for( const GraphNode& node : nodes_ )
		{
			for( const NodePin& pin : node.inputs )
				if( pin.id == pinId )
					return &pin;
			for( const NodePin& pin : node.outputs )
				if( pin.id == pinId )
					return &pin;
		}
		return nullptr;
	}

	const GraphLink* NodeGraph::LinkForInput( PinId inputPin ) const
	{
		const auto found = std::find_if( links_.begin(), links_.end(), [ inputPin ]( const GraphLink& link ) { return link.inputPin == inputPin; } );
		return found == links_.end() ? nullptr : &*found;
	}

	EvaluationResult NodeGraph::Evaluate( NodeId nodeId ) const
	{
		std::vector<NodeId> stack;
		return EvaluateNode( nodeId, stack );
	}

	std::vector<GraphNode>& NodeGraph::Nodes() noexcept
	{
		return nodes_;
	}
	const std::vector<GraphNode>& NodeGraph::Nodes() const noexcept
	{
		return nodes_;
	}
	const std::vector<GraphLink>& NodeGraph::Links() const noexcept
	{
		return links_;
	}

	NodePin NodeGraph::MakePin( NodeId nodeId, std::string name, NodeValueType type, bool input )
	{
		return { nextPinId_++, nodeId, std::move( name ), type, input };
	}

	EvaluationResult NodeGraph::EvaluateNode( NodeId nodeId, std::vector<NodeId>& stack ) const
	{
		if( std::find( stack.begin(), stack.end(), nodeId ) != stack.end() )
			return Error( "Ciclo detectado durante la evaluacion." );
		const GraphNode* node = FindNode( nodeId );
		if( !node )
			return Error( "Nodo no encontrado." );
		stack.push_back( nodeId );
		auto finish = [ &stack ]( EvaluationResult result )
		{
			stack.pop_back();
			return result;
		};

		if( node->kind == NodeKind::FloatConstant )
			return finish( FloatResult( node->constantValue ) );
		if( node->kind == NodeKind::TextureSource )
			return finish( TextureResult( node->textureValue ) );
		if( node->kind == NodeKind::TextureCpuModifier )
		{
			const EvaluationResult input = EvaluateInput( node->inputs[ 0 ], stack );
			if( !input.valid || !std::holds_alternative<TextureReference>( input.value ) )
				return finish( Error( "Modificar textura CPU necesita Texture2D." ) );
			return finish( input );
		}
		if( node->kind == NodeKind::TexturePreview )
		{
			const EvaluationResult input = EvaluateInput( node->inputs[ 0 ], stack );
			if( !input.valid )
				return finish( input );
			if( !std::holds_alternative<TextureReference>( input.value ) )
				return finish( Error( "Visualizar textura necesita Texture2D." ) );
			return finish( input );
		}
		if( node->kind == NodeKind::ComposeVector3 )
		{
			const EvaluationResult x = EvaluateInput( node->inputs[ 0 ], stack );
			if( !x.valid )
				return finish( x );
			const EvaluationResult y = EvaluateInput( node->inputs[ 1 ], stack );
			if( !y.valid )
				return finish( y );
			const EvaluationResult z = EvaluateInput( node->inputs[ 2 ], stack );
			if( !z.valid )
				return finish( z );
			return finish( VectorResult( { std::get<float>( x.value ), std::get<float>( y.value ), std::get<float>( z.value ) } ) );
		}
		if( node->kind == NodeKind::VectorLength )
		{
			const EvaluationResult input = EvaluateInput( node->inputs[ 0 ], stack );
			if( !input.valid )
				return finish( input );
			if( !std::holds_alternative<Float3>( input.value ) )
				return finish( Error( "VectorLength necesita un Vector3." ) );
			const Float3 vector = std::get<Float3>( input.value );
			return finish( FloatResult( std::sqrt( vector.x * vector.x + vector.y * vector.y + vector.z * vector.z ) ) );
		}

		const EvaluationResult a = EvaluateInput( node->inputs[ 0 ], stack );
		if( !a.valid )
			return finish( a );
		const EvaluationResult b = EvaluateInput( node->inputs[ 1 ], stack );
		if( !b.valid )
			return finish( b );
		if( !std::holds_alternative<float>( a.value ) || !std::holds_alternative<float>( b.value ) )
			return finish( Error( "La operacion necesita valores float." ) );
		const float left = std::get<float>( a.value );
		const float right = std::get<float>( b.value );
		switch( node->kind )
		{
		case NodeKind::Add:
			return finish( FloatResult( left + right ) );
		case NodeKind::Subtract:
			return finish( FloatResult( left - right ) );
		case NodeKind::Multiply:
			return finish( FloatResult( left * right ) );
		default:
			return finish( Error( "Tipo de nodo no evaluable." ) );
		}
	}

	EvaluationResult NodeGraph::EvaluateInput( const NodePin& pin, std::vector<NodeId>& stack ) const
	{
		const GraphLink* link = LinkForInput( pin.id );
		if( !link )
		{
			if( pin.type == NodeValueType::Float )
				return FloatResult( 0.0f );
			if( pin.type == NodeValueType::Vector3 )
				return VectorResult( {} );
			return TextureResult( {} );
		}
		const NodePin* output = FindPin( link->outputPin );
		if( !output )
			return Error( "La conexion apunta a un pin inexistente." );
		return EvaluateNode( output->nodeId, stack );
	}

	bool NodeGraph::WouldCreateCycle( NodeId sourceNode, NodeId destinationNode ) const
	{
		std::vector<NodeId> pending{ destinationNode };
		std::unordered_set<NodeId> visited;
		while( !pending.empty() )
		{
			const NodeId current = pending.back();
			pending.pop_back();
			if( current == sourceNode )
				return true;
			if( !visited.insert( current ).second )
				continue;
			for( const GraphLink& link : links_ )
			{
				const NodePin* output = FindPin( link.outputPin );
				const NodePin* input = FindPin( link.inputPin );
				if( output && input && output->nodeId == current )
					pending.push_back( input->nodeId );
			}
		}
		return false;
	}

	const char* NodeKindName( NodeKind kind ) noexcept
	{
		switch( kind )
		{
		case NodeKind::FloatConstant:
			return "Constante float";
		case NodeKind::Add:
			return "Suma";
		case NodeKind::Subtract:
			return "Resta";
		case NodeKind::Multiply:
			return "Multiplicacion";
		case NodeKind::ComposeVector3:
			return "Montar Vector3";
		case NodeKind::VectorLength:
			return "Longitud Vector3";
		case NodeKind::TextureSource:
			return "Textura de ejemplo";
		case NodeKind::TextureCpuModifier:
			return "Modificar textura CPU";
		case NodeKind::TexturePreview:
			return "Visualizar textura";
		}
		return "Nodo";
	}

	const char* NodeValueTypeName( NodeValueType type ) noexcept
	{
		switch( type )
		{
		case NodeValueType::Float:
			return "float";
		case NodeValueType::Vector3:
			return "Vector3";
		case NodeValueType::Texture2D:
			return "Texture2D";
		}
		return "?";
	}

	std::string NodeValueText( const NodeValue& value )
	{
		std::ostringstream stream;
		stream << std::fixed << std::setprecision( 3 );
		if( const auto* scalar = std::get_if<float>( &value ) )
			stream << *scalar;
		else if( const auto* vector = std::get_if<Float3>( &value ) )
		{
			stream << "(" << vector->x << ", " << vector->y << ", " << vector->z << ")";
		}
		else
		{
			const TextureReference texture = std::get<TextureReference>( value );
			stream << "Texture2D " << texture.width << "x" << texture.height << " (#" << texture.resourceId << ")";
		}
		return stream.str();
	}
}
