#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace App
{
	using NodeId = uint64_t;
	using PinId = uint64_t;
	using LinkId = uint64_t;

	struct Float3
	{
		float x = 0.0f;
		float y = 0.0f;
		float z = 0.0f;
	};

	struct TextureReference
	{
		uint64_t resourceId = 0;
		uint32_t width = 0;
		uint32_t height = 0;
	};

	enum class NodeValueType : uint8_t
	{
		Float,
		Vector3,
		Texture2D
	};

	using NodeValue = std::variant<float, Float3, TextureReference>;

	enum class NodeKind : uint8_t
	{
		FloatConstant,
		Add,
		Subtract,
		Multiply,
		ComposeVector3,
		VectorLength,
		TextureSource,
		TextureCpuModifier,
		TexturePreview
	};

	struct NodePin
	{
		PinId id = 0;
		NodeId nodeId = 0;
		std::string name;
		NodeValueType type = NodeValueType::Float;
		bool input = true;
	};

	struct GraphNode
	{
		NodeId id = 0;
		NodeKind kind = NodeKind::FloatConstant;
		std::string title;
		std::vector<NodePin> inputs;
		std::vector<NodePin> outputs;
		float constantValue = 0.0f;
		TextureReference textureValue{};
	};

	struct GraphLink
	{
		LinkId id = 0;
		PinId outputPin = 0;
		PinId inputPin = 0;
	};

	struct EvaluationResult
	{
		bool valid = false;
		NodeValue value = 0.0f;
		std::string error;
	};

	class NodeGraph final
	{
	public:
		NodeId AddNode( NodeKind kind, float initialValue = 0.0f );
		bool RemoveNode( NodeId nodeId );
		bool Connect( PinId firstPin, PinId secondPin, std::string* error = nullptr );
		bool RemoveLink( LinkId linkId );
		void Clear();

		GraphNode* FindNode( NodeId nodeId );
		const GraphNode* FindNode( NodeId nodeId ) const;
		const NodePin* FindPin( PinId pinId ) const;
		const GraphLink* LinkForInput( PinId inputPin ) const;
		EvaluationResult Evaluate( NodeId nodeId ) const;

		std::vector<GraphNode>& Nodes() noexcept;
		const std::vector<GraphNode>& Nodes() const noexcept;
		const std::vector<GraphLink>& Links() const noexcept;

	private:
		NodePin MakePin( NodeId nodeId, std::string name, NodeValueType type, bool input );
		EvaluationResult EvaluateNode( NodeId nodeId, std::vector<NodeId>& stack ) const;
		EvaluationResult EvaluateInput( const NodePin& pin, std::vector<NodeId>& stack ) const;
		bool WouldCreateCycle( NodeId sourceNode, NodeId destinationNode ) const;

		std::vector<GraphNode> nodes_;
		std::vector<GraphLink> links_;
		NodeId nextNodeId_ = 1;
		PinId nextPinId_ = 1;
		LinkId nextLinkId_ = 1;
	};

	const char* NodeKindName( NodeKind kind ) noexcept;
	const char* NodeValueTypeName( NodeValueType type ) noexcept;
	std::string NodeValueText( const NodeValue& value );
}
