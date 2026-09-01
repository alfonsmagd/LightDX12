#define IMGUI_DEFINE_MATH_OPERATORS

#include "App/ImGuiLayer.hpp"
#include "App/NodeGraph.hpp"
#include "App/imgui_impl_ldx12.h"
#include "Ldx12/Ldx12.hpp"
#include "Ldx12Utils/AppLdx.hpp"

#include "imgui.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

using namespace ldx12;

namespace
{
	constexpr float kNodeWidth = 190.0f;
	constexpr float kHeaderHeight = 30.0f;
	constexpr float kPinRowHeight = 26.0f;
	constexpr uint32_t kFramesInFlight = 3;

	struct NodeLayout
	{
		ImVec2 minimum{};
		ImVec2 maximum{};
		ImVec2 contentTop{};
		float resultY = 0.0f;
	};

	struct EditorState
	{
		App::NodeGraph graph;
		std::unordered_map<App::NodeId, ImVec2> positions;
		std::unordered_map<App::PinId, ImVec2> pinPositions;
		std::optional<App::NodeId> selectedNode;
		std::optional<App::NodeId> draggedNode;
		std::optional<App::PinId> draggedPin;
		ImVec2 nodeDragOffset{};
		ImVec2 pan{ 32.0f, 48.0f };
		ImVec2 popupGraphPosition{};
		std::string status = "Arrastra desde un pin hasta otro para conectarlos.";
		uint32_t placementIndex = 0;
		bool valueEditorHovered = false;
		bool cpuInvertRequested = false;
		App::TextureReference demoTexture{ 1, 256, 256 };
		TextureHandle imguiTexture{};
	};

	struct GraphicsState
	{
		std::unique_ptr<App::ImGuiLayer> imgui;
		TextureHandle demoTexture{};
		std::vector<uint32_t> cpuPixels;
		EditorState editor;
		std::array<SubmitHandle, kFramesInFlight> frameSubmissions{};
		uint32_t frameIndex = 0;
	};

	ImU32 PinColor( App::NodeValueType type )
	{
		switch( type )
		{
		case App::NodeValueType::Float:
			return IM_COL32( 77, 166, 255, 255 );
		case App::NodeValueType::Vector3:
			return IM_COL32( 255, 166, 66, 255 );
		case App::NodeValueType::Texture2D:
			return IM_COL32( 92, 214, 139, 255 );
		}
		return IM_COL32_WHITE;
	}

	ImU32 NodeHeaderColor( App::NodeKind kind )
	{
		switch( kind )
		{
		case App::NodeKind::FloatConstant:
			return IM_COL32( 67, 91, 119, 255 );
		case App::NodeKind::Add:
			return IM_COL32( 38, 125, 91, 255 );
		case App::NodeKind::Subtract:
			return IM_COL32( 145, 83, 48, 255 );
		case App::NodeKind::Multiply:
			return IM_COL32( 117, 69, 145, 255 );
		case App::NodeKind::ComposeVector3:
			return IM_COL32( 166, 105, 35, 255 );
		case App::NodeKind::VectorLength:
			return IM_COL32( 42, 113, 143, 255 );
		case App::NodeKind::TextureSource:
			return IM_COL32( 58, 122, 79, 255 );
		case App::NodeKind::TextureCpuModifier:
			return IM_COL32( 128, 82, 40, 255 );
		case App::NodeKind::TexturePreview:
			return IM_COL32( 45, 137, 104, 255 );
		}
		return IM_COL32( 80, 80, 80, 255 );
	}

	float NodeContentOffset( const App::GraphNode& node )
	{
		return node.kind == App::NodeKind::FloatConstant || node.kind == App::NodeKind::TextureCpuModifier ? 46.0f : 12.0f;
	}

	NodeLayout CalculateLayout( const App::GraphNode& node, const EditorState& editor, ImVec2 canvasOrigin )
	{
		const auto found = editor.positions.find( node.id );
		const ImVec2 graphPosition = found != editor.positions.end() ? found->second : ImVec2{};
		const size_t rows = std::max<size_t>( 1, std::max( node.inputs.size(), node.outputs.size() ) );
		const float contentOffset = NodeContentOffset( node );
		const float previewHeight = node.kind == App::NodeKind::TexturePreview ? 128.0f : 0.0f;
		const float height = kHeaderHeight + contentOffset + static_cast<float>( rows ) * kPinRowHeight + previewHeight + 32.0f;
		NodeLayout layout;
		layout.minimum = canvasOrigin + editor.pan + graphPosition;
		layout.maximum = layout.minimum + ImVec2( kNodeWidth, height );
		layout.contentTop = layout.minimum + ImVec2( 0.0f, kHeaderHeight + contentOffset );
		layout.resultY = layout.contentTop.y + static_cast<float>( rows ) * kPinRowHeight + previewHeight + 4.0f;
		return layout;
	}

	void DrawConnection( ImDrawList* drawList, ImVec2 output, ImVec2 input, ImU32 color, float thickness = 3.0f )
	{
		const float tangent = std::max( 48.0f, std::abs( input.x - output.x ) * 0.45f );
		drawList->AddBezierCubic( output, output + ImVec2( tangent, 0.0f ), input - ImVec2( tangent, 0.0f ), input, color, thickness );
	}

	App::NodeId AddEditorNode( EditorState& editor, App::NodeKind kind, ImVec2 graphPosition, float value = 0.0f )
	{
		const App::NodeId id = editor.graph.AddNode( kind, value );
		editor.positions[ id ] = graphPosition;
		if( kind == App::NodeKind::TextureSource )
			editor.graph.FindNode( id )->textureValue = editor.demoTexture;
		editor.selectedNode = id;
		return id;
	}

	void ConnectFirstOutput( EditorState& editor, App::NodeId outputNode, App::NodeId inputNode, size_t inputIndex )
	{
		const App::GraphNode* source = editor.graph.FindNode( outputNode );
		const App::GraphNode* destination = editor.graph.FindNode( inputNode );
		if( source && destination && !source->outputs.empty() && inputIndex < destination->inputs.size() )
			editor.graph.Connect( source->outputs[ 0 ].id, destination->inputs[ inputIndex ].id );
	}

	void BuildExampleGraph( EditorState& editor )
	{
		editor.graph.Clear();
		editor.positions.clear();
		const App::NodeId valueA = AddEditorNode( editor, App::NodeKind::FloatConstant, { 10, 40 }, 3.0f );
		const App::NodeId valueB = AddEditorNode( editor, App::NodeKind::FloatConstant, { 10, 230 }, 2.0f );
		const App::NodeId add = AddEditorNode( editor, App::NodeKind::Add, { 260, 30 } );
		const App::NodeId subtract = AddEditorNode( editor, App::NodeKind::Subtract, { 260, 220 } );
		const App::NodeId multiply = AddEditorNode( editor, App::NodeKind::Multiply, { 510, 220 } );
		const App::NodeId vector = AddEditorNode( editor, App::NodeKind::ComposeVector3, { 510, 20 } );
		const App::NodeId length = AddEditorNode( editor, App::NodeKind::VectorLength, { 770, 70 } );
		const App::NodeId texture = AddEditorNode( editor, App::NodeKind::TextureSource, { 10, 450 } );
		const App::NodeId cpuModifier = AddEditorNode( editor, App::NodeKind::TextureCpuModifier, { 260, 450 } );
		const App::NodeId preview = AddEditorNode( editor, App::NodeKind::TexturePreview, { 510, 430 } );
		ConnectFirstOutput( editor, valueA, add, 0 );
		ConnectFirstOutput( editor, valueB, add, 1 );
		ConnectFirstOutput( editor, valueA, subtract, 0 );
		ConnectFirstOutput( editor, valueB, subtract, 1 );
		ConnectFirstOutput( editor, add, multiply, 0 );
		ConnectFirstOutput( editor, subtract, multiply, 1 );
		ConnectFirstOutput( editor, add, vector, 0 );
		ConnectFirstOutput( editor, subtract, vector, 1 );
		ConnectFirstOutput( editor, multiply, vector, 2 );
		ConnectFirstOutput( editor, vector, length, 0 );
		ConnectFirstOutput( editor, texture, cpuModifier, 0 );
		ConnectFirstOutput( editor, cpuModifier, preview, 0 );
		editor.selectedNode = length;
		editor.status = "Ejemplo: Vector3(Suma, Resta, Multiplicacion) y su longitud.";
	}

	void DrawNode( EditorState& editor, App::GraphNode& node, const NodeLayout& layout, ImDrawList* drawList )
	{
		const bool selected = editor.selectedNode && *editor.selectedNode == node.id;
		drawList->AddRectFilled( layout.minimum + ImVec2( 4, 5 ), layout.maximum + ImVec2( 4, 5 ), IM_COL32( 0, 0, 0, 75 ), 7.0f );
		drawList->AddRectFilled( layout.minimum, layout.maximum, IM_COL32( 31, 35, 43, 248 ), 7.0f );
		drawList->AddRectFilled( layout.minimum,
			ImVec2( layout.maximum.x, layout.minimum.y + kHeaderHeight ),
			NodeHeaderColor( node.kind ),
			7.0f,
			ImDrawFlags_RoundCornersTop );
		drawList->AddRect( layout.minimum,
			layout.maximum,
			selected ? IM_COL32( 255, 208, 92, 255 ) : IM_COL32( 83, 91, 107, 255 ),
			7.0f,
			0,
			selected ? 2.5f : 1.0f );
		drawList->AddText( layout.minimum + ImVec2( 10, 7 ), IM_COL32_WHITE, node.title.c_str() );

		if( node.kind == App::NodeKind::FloatConstant )
		{
			ImGui::SetCursorScreenPos( layout.minimum + ImVec2( 10, kHeaderHeight + 8 ) );
			ImGui::PushID( static_cast<int>( node.id ) );
			ImGui::SetNextItemWidth( kNodeWidth - 20.0f );
			ImGui::InputFloat( "##value", &node.constantValue, 0.1f, 1.0f, "%.3f" );
			editor.valueEditorHovered = editor.valueEditorHovered || ImGui::IsItemHovered() || ImGui::IsItemActive();
			ImGui::PopID();
		}
		else if( node.kind == App::NodeKind::TextureCpuModifier )
		{
			ImGui::SetCursorScreenPos( layout.minimum + ImVec2( 10, kHeaderHeight + 8 ) );
			ImGui::PushID( static_cast<int>( node.id ) );
			if( ImGui::Button( "Invertir RGB en CPU", ImVec2( kNodeWidth - 20.0f, 0.0f ) ) )
				editor.cpuInvertRequested = true;
			editor.valueEditorHovered = editor.valueEditorHovered || ImGui::IsItemHovered() || ImGui::IsItemActive();
			ImGui::PopID();
		}

		for( size_t index = 0; index < node.inputs.size(); ++index )
		{
			const App::NodePin& pin = node.inputs[ index ];
			const ImVec2 position( layout.minimum.x, layout.contentTop.y + static_cast<float>( index ) * kPinRowHeight + kPinRowHeight * 0.5f );
			editor.pinPositions[ pin.id ] = position;
			drawList->AddCircleFilled( position, 6.0f, PinColor( pin.type ) );
			drawList->AddCircle( position, 8.0f, IM_COL32( 220, 225, 235, 180 ), 0, 1.0f );
			drawList->AddText( position + ImVec2( 12, -8 ), IM_COL32( 220, 225, 235, 255 ), pin.name.c_str() );
		}
		for( size_t index = 0; index < node.outputs.size(); ++index )
		{
			const App::NodePin& pin = node.outputs[ index ];
			const ImVec2 position( layout.maximum.x, layout.contentTop.y + static_cast<float>( index ) * kPinRowHeight + kPinRowHeight * 0.5f );
			editor.pinPositions[ pin.id ] = position;
			drawList->AddCircleFilled( position, 6.0f, PinColor( pin.type ) );
			drawList->AddCircle( position, 8.0f, IM_COL32( 220, 225, 235, 180 ), 0, 1.0f );
			const ImVec2 textSize = ImGui::CalcTextSize( pin.name.c_str() );
			drawList->AddText( position + ImVec2( -textSize.x - 12, -8 ), IM_COL32( 220, 225, 235, 255 ), pin.name.c_str() );
		}

		const App::EvaluationResult result = editor.graph.Evaluate( node.id );
		if( node.kind == App::NodeKind::TexturePreview )
		{
			const float pinsBottom =
				layout.contentTop.y + static_cast<float>( std::max<size_t>( 1, std::max( node.inputs.size(), node.outputs.size() ) ) ) * kPinRowHeight;
			const ImVec2 imageMinimum( layout.minimum.x + 10.0f, pinsBottom + 4.0f );
			const ImVec2 imageMaximum( layout.maximum.x - 10.0f, pinsBottom + 118.0f );
			const auto* texture = result.valid ? std::get_if<App::TextureReference>( &result.value ) : nullptr;
			if( texture && texture->resourceId != 0 && editor.imguiTexture.Valid() )
				drawList->AddImage( ImGui_ImplLdx12_Texture( editor.imguiTexture ), imageMinimum, imageMaximum );
			else
			{
				drawList->AddRectFilled( imageMinimum, imageMaximum, IM_COL32( 17, 19, 24, 255 ), 4.0f );
				const char* text = "Conecta una Texture2D";
				const ImVec2 size = ImGui::CalcTextSize( text );
				drawList->AddText( ( imageMinimum + imageMaximum - size ) * 0.5f, IM_COL32( 150, 158, 170, 255 ), text );
			}
			drawList->AddRect( imageMinimum, imageMaximum, IM_COL32( 100, 110, 125, 255 ), 4.0f );
		}
		const std::string resultText = result.valid ? "= " + App::NodeValueText( result.value ) : "Error: " + result.error;
		drawList->AddLine( ImVec2( layout.minimum.x + 8, layout.resultY - 3 ),
			ImVec2( layout.maximum.x - 8, layout.resultY - 3 ),
			IM_COL32( 75, 82, 96, 255 ) );
		drawList->AddText( ImVec2( layout.minimum.x + 10, layout.resultY + 4 ),
			result.valid ? IM_COL32( 155, 224, 175, 255 ) : IM_COL32( 255, 105, 105, 255 ),
			resultText.c_str() );
	}

	std::optional<App::PinId> FindHoveredPin( const EditorState& editor, ImVec2 mouse )
	{
		for( const auto& [ pinId, position ] : editor.pinPositions )
		{
			const ImVec2 delta = mouse - position;
			if( delta.x * delta.x + delta.y * delta.y <= 100.0f )
				return pinId;
		}
		return std::nullopt;
	}

	void CreateNodeMenu( EditorState& editor )
	{
		if( !ImGui::BeginPopup( "CreateNode" ) )
			return;
		ImGui::TextDisabled( "Crear nodo" );
		ImGui::Separator();
		const auto add = [ &editor ]( const char* label, App::NodeKind kind, float value = 0.0f )
		{
			if( ImGui::MenuItem( label ) )
				AddEditorNode( editor, kind, editor.popupGraphPosition, value );
		};
		add( "Constante float", App::NodeKind::FloatConstant, 1.0f );
		add( "Suma", App::NodeKind::Add );
		add( "Resta", App::NodeKind::Subtract );
		add( "Multiplicacion", App::NodeKind::Multiply );
		ImGui::Separator();
		add( "Montar Vector3 (X, Y, Z)", App::NodeKind::ComposeVector3 );
		add( "Longitud Vector3", App::NodeKind::VectorLength );
		ImGui::Separator();
		add( "Textura de ejemplo", App::NodeKind::TextureSource );
		add( "Invertir textura CPU", App::NodeKind::TextureCpuModifier );
		add( "Visualizar textura", App::NodeKind::TexturePreview );
		ImGui::EndPopup();
	}

	void DrawNodeCanvas( EditorState& editor )
	{
		ImGui::BeginChild( "NodeCanvas", ImVec2( 0, 0 ), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse );
		const ImVec2 canvasOrigin = ImGui::GetCursorScreenPos();
		ImVec2 canvasSize = ImGui::GetContentRegionAvail();
		canvasSize.x = std::max( canvasSize.x, 64.0f );
		canvasSize.y = std::max( canvasSize.y, 64.0f );
		const ImVec2 canvasMaximum = canvasOrigin + canvasSize;
		// Reserve canvas space without an active full-size button. An
		// InvisibleButton here would capture clicks before overlaid InputFloat
		// controls can receive keyboard focus.
		ImGui::Dummy( canvasSize );
		const bool canvasHovered =
			ImGui::IsWindowHovered( ImGuiHoveredFlags_AllowWhenBlockedByActiveItem ) && ImGui::IsMouseHoveringRect( canvasOrigin, canvasMaximum );
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		drawList->PushClipRect( canvasOrigin, canvasMaximum, true );
		drawList->AddRectFilled( canvasOrigin, canvasMaximum, IM_COL32( 19, 22, 28, 255 ) );

		constexpr float gridSpacing = 32.0f;
		for( float x = std::fmod( editor.pan.x, gridSpacing ); x < canvasSize.x; x += gridSpacing )
			drawList->AddLine( canvasOrigin + ImVec2( x, 0 ), canvasOrigin + ImVec2( x, canvasSize.y ), IM_COL32( 54, 60, 70, 95 ) );
		for( float y = std::fmod( editor.pan.y, gridSpacing ); y < canvasSize.y; y += gridSpacing )
			drawList->AddLine( canvasOrigin + ImVec2( 0, y ), canvasOrigin + ImVec2( canvasSize.x, y ), IM_COL32( 54, 60, 70, 95 ) );

		std::unordered_map<App::NodeId, NodeLayout> layouts;
		editor.pinPositions.clear();
		editor.valueEditorHovered = false;
		for( const App::GraphNode& node : editor.graph.Nodes() )
		{
			const NodeLayout layout = CalculateLayout( node, editor, canvasOrigin );
			layouts[ node.id ] = layout;
			for( size_t index = 0; index < node.inputs.size(); ++index )
				editor.pinPositions[ node.inputs[ index ].id ] = { layout.minimum.x,
					layout.contentTop.y + static_cast<float>( index ) * kPinRowHeight + kPinRowHeight * 0.5f };
			for( size_t index = 0; index < node.outputs.size(); ++index )
				editor.pinPositions[ node.outputs[ index ].id ] = { layout.maximum.x,
					layout.contentTop.y + static_cast<float>( index ) * kPinRowHeight + kPinRowHeight * 0.5f };
		}

		for( const App::GraphLink& link : editor.graph.Links() )
		{
			const auto output = editor.pinPositions.find( link.outputPin );
			const auto input = editor.pinPositions.find( link.inputPin );
			const App::NodePin* pin = editor.graph.FindPin( link.outputPin );
			if( output != editor.pinPositions.end() && input != editor.pinPositions.end() && pin )
				DrawConnection( drawList, output->second, input->second, PinColor( pin->type ) );
		}

		for( App::GraphNode& node : editor.graph.Nodes() )
			DrawNode( editor, node, layouts[ node.id ], drawList );

		const ImVec2 mouse = ImGui::GetIO().MousePos;
		const std::optional<App::PinId> hoveredPin = FindHoveredPin( editor, mouse );
		if( hoveredPin )
		{
			const App::NodePin* pin = editor.graph.FindPin( *hoveredPin );
			if( pin )
			{
				drawList->AddCircle( editor.pinPositions[ *hoveredPin ], 10.0f, IM_COL32_WHITE, 0, 2.0f );
				ImGui::SetTooltip( "%s: %s", pin->name.c_str(), App::NodeValueTypeName( pin->type ) );
			}
		}

		if( editor.draggedPin )
		{
			const App::NodePin* pin = editor.graph.FindPin( *editor.draggedPin );
			const ImVec2 start = editor.pinPositions[ *editor.draggedPin ];
			if( pin && pin->input )
				DrawConnection( drawList, mouse, start, PinColor( pin->type ), 2.0f );
			else if( pin )
				DrawConnection( drawList, start, mouse, PinColor( pin->type ), 2.0f );
		}

		if( canvasHovered && !editor.valueEditorHovered && ImGui::IsMouseDragging( ImGuiMouseButton_Middle, 0.0f ) )
			editor.pan = editor.pan + ImGui::GetIO().MouseDelta;
		if( !editor.valueEditorHovered && ImGui::IsMouseClicked( ImGuiMouseButton_Left ) )
		{
			if( hoveredPin )
				editor.draggedPin = hoveredPin;
			else
			{
				editor.selectedNode.reset();
				for( auto iterator = editor.graph.Nodes().rbegin(); iterator != editor.graph.Nodes().rend(); ++iterator )
				{
					const NodeLayout& layout = layouts[ iterator->id ];
					if( ImGui::IsMouseHoveringRect( layout.minimum, ImVec2( layout.maximum.x, layout.minimum.y + kHeaderHeight ) ) )
					{
						editor.selectedNode = iterator->id;
						editor.draggedNode = iterator->id;
						editor.nodeDragOffset = mouse - layout.minimum;
						break;
					}
				}
			}
		}
		if( editor.draggedNode && ImGui::IsMouseDown( ImGuiMouseButton_Left ) )
			editor.positions[ *editor.draggedNode ] = mouse - canvasOrigin - editor.pan - editor.nodeDragOffset;
		if( ImGui::IsMouseReleased( ImGuiMouseButton_Left ) )
		{
			if( editor.draggedPin )
			{
				if( hoveredPin && *hoveredPin != *editor.draggedPin )
				{
					std::string error;
					if( editor.graph.Connect( *editor.draggedPin, *hoveredPin, &error ) )
						editor.status = "Conexion creada.";
					else
						editor.status = error;
				}
				editor.draggedPin.reset();
			}
			editor.draggedNode.reset();
		}

		if( !editor.valueEditorHovered && hoveredPin && ImGui::IsMouseClicked( ImGuiMouseButton_Right ) )
		{
			const App::NodePin* pin = editor.graph.FindPin( *hoveredPin );
			const App::GraphLink* link = pin && pin->input ? editor.graph.LinkForInput( pin->id ) : nullptr;
			if( link )
			{
				editor.graph.RemoveLink( link->id );
				editor.status = "Conexion eliminada.";
			}
		}
		else if( !editor.valueEditorHovered && canvasHovered && ImGui::IsMouseClicked( ImGuiMouseButton_Right ) )
		{
			editor.popupGraphPosition = mouse - canvasOrigin - editor.pan;
			ImGui::OpenPopup( "CreateNode" );
		}

		if( editor.selectedNode && ImGui::IsWindowFocused( ImGuiFocusedFlags_RootAndChildWindows ) && !ImGui::IsAnyItemActive() &&
			ImGui::IsKeyPressed( ImGuiKey_Delete ) )
		{
			editor.graph.RemoveNode( *editor.selectedNode );
			editor.positions.erase( *editor.selectedNode );
			editor.selectedNode.reset();
			editor.status = "Nodo eliminado.";
		}

		CreateNodeMenu( editor );
		drawList->PopClipRect();
		ImGui::EndChild();
	}

	void DrawPalette( EditorState& editor )
	{
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos( viewport->WorkPos );
		ImGui::SetNextWindowSize( ImVec2( 270.0f, viewport->WorkSize.y ) );
		ImGui::Begin( "Nodos", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse );
		ImGui::TextWrapped( "Ejemplo de grafo tipado sobre ImGui y App::NodeGraph." );
		ImGui::SeparatorText( "Crear nodo" );
		const auto addButton = [ &editor ]( const char* label, App::NodeKind kind, float value = 0.0f )
		{
			if( ImGui::Button( label, ImVec2( -1, 0 ) ) )
			{
				const float offset = static_cast<float>( editor.placementIndex++ % 8 ) * 28.0f;
				AddEditorNode( editor, kind, { 60.0f + offset, 70.0f + offset }, value );
			}
		};
		addButton( "Constante float", App::NodeKind::FloatConstant, 1.0f );
		addButton( "Suma (A + B)", App::NodeKind::Add );
		addButton( "Resta (A - B)", App::NodeKind::Subtract );
		addButton( "Multiplicacion (A * B)", App::NodeKind::Multiply );
		addButton( "Vector3 (X, Y, Z)", App::NodeKind::ComposeVector3 );
		addButton( "Longitud de Vector3", App::NodeKind::VectorLength );
		addButton( "Textura de ejemplo", App::NodeKind::TextureSource );
		addButton( "Invertir textura CPU", App::NodeKind::TextureCpuModifier );
		addButton( "Visualizar textura", App::NodeKind::TexturePreview );
		ImGui::Spacing();
		if( ImGui::Button( "Restaurar ejemplo", ImVec2( -1, 0 ) ) )
			BuildExampleGraph( editor );
		if( ImGui::Button( "Limpiar grafo", ImVec2( -1, 0 ) ) )
		{
			editor.graph.Clear();
			editor.positions.clear();
			editor.selectedNode.reset();
		}

		ImGui::SeparatorText( "Estado" );
		ImGui::Text( "Nodos: %zu", editor.graph.Nodes().size() );
		ImGui::Text( "Conexiones: %zu", editor.graph.Links().size() );
		ImGui::TextWrapped( "%s", editor.status.c_str() );
		if( editor.selectedNode )
		{
			const App::GraphNode* node = editor.graph.FindNode( *editor.selectedNode );
			if( node )
			{
				const App::EvaluationResult result = editor.graph.Evaluate( node->id );
				ImGui::SeparatorText( "Seleccion" );
				ImGui::Text( "%s (#%llu)", node->title.c_str(), static_cast<unsigned long long>( node->id ) );
				if( result.valid )
					ImGui::TextWrapped( "Resultado: %s", App::NodeValueText( result.value ).c_str() );
				else
					ImGui::TextColored( ImVec4( 1, 0.35f, 0.35f, 1 ), "%s", result.error.c_str() );
			}
		}

		ImGui::SeparatorText( "Controles" );
		ImGui::BulletText( "Arrastra la cabecera: mover nodo" );
		ImGui::BulletText( "Arrastra un pin: crear enlace" );
		ImGui::BulletText( "Boton central: desplazar canvas" );
		ImGui::BulletText( "Clic derecho: crear/desconectar" );
		ImGui::BulletText( "Supr: eliminar nodo seleccionado" );
		ImGui::End();
	}

	void DrawEditor( EditorState& editor )
	{
		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos( viewport->WorkPos + ImVec2( 270.0f, 0.0f ) );
		ImGui::SetNextWindowSize( ImVec2( viewport->WorkSize.x - 270.0f, viewport->WorkSize.y ) );
		ImGui::Begin( "Editor de nodos", nullptr, ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse );
		DrawNodeCanvas( editor );
		ImGui::End();
	}

	std::vector<uint32_t> CreateDemoPixels()
	{
		constexpr uint32_t width = 256;
		constexpr uint32_t height = 256;
		std::vector<uint32_t> pixels( static_cast<size_t>( width ) * height );
		for( uint32_t y = 0; y < height; ++y )
		{
			for( uint32_t x = 0; x < width; ++x )
			{
				const bool checker = ( ( x / 32 ) + ( y / 32 ) ) % 2 == 0;
				const uint32_t red = checker ? 42u + x * 180u / width : 18u;
				const uint32_t green = checker ? 95u + y * 140u / height : 42u;
				const uint32_t blue = checker ? 210u : 92u + x * 100u / width;
				pixels[ static_cast<size_t>( y ) * width + x ] = red | ( green << 8u ) | ( blue << 16u ) | 0xff000000u;
			}
		}
		return pixels;
	}

	TextureHandle UploadCpuTexture( RenderDevice& device, const std::vector<uint32_t>& pixels )
	{
		constexpr uint32_t width = 256;
		constexpr uint32_t height = 256;
		TextureDesc desc{};
		desc.debugName = "Node Editor CPU Texture";
		desc.width = width;
		desc.height = height;
		desc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.usage = TextureUsage::Sampled;
		desc.initialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		desc.data = pixels.data();
		desc.rowPitch = width * sizeof( uint32_t );
		desc.slicePitch = desc.rowPitch * height;
		return device.CreateTexture( desc );
	}

	void ApplyCpuInvert( GraphicsState& gfx, RenderDevice& device )
	{
		// This is the CPU modification: decode each RGBA8 pixel, invert RGB and
		// keep alpha unchanged. No shader or GPU compute work is involved.
		for( uint32_t& pixel : gfx.cpuPixels )
		{
			const uint32_t red = 255u - ( pixel & 0xffu );
			const uint32_t green = 255u - ( ( pixel >> 8u ) & 0xffu );
			const uint32_t blue = 255u - ( ( pixel >> 16u ) & 0xffu );
			pixel = red | ( green << 8u ) | ( blue << 16u ) | ( pixel & 0xff000000u );
		}

		device.WaitIdle();
		device.Destroy( gfx.demoTexture );
		gfx.demoTexture = UploadCpuTexture( device, gfx.cpuPixels );
		gfx.editor.imguiTexture = gfx.demoTexture;
		gfx.editor.status = "Textura invertida recorriendo 65.536 pixels en CPU.";
		gfx.editor.cpuInvertRequested = false;
	}

	bool HandleImGuiMessage( HWND window, UINT message, WPARAM wParam, LPARAM lParam, void* )
	{
		return App::ImGuiLayer::HandleMessage( window, message, wParam, lParam );
	}
}

int WINAPI wWinMain( HINSTANCE instance, HINSTANCE, PWSTR, int showCommand )
{
	try
	{
		constexpr uint32_t initialWidth = 1500;
		constexpr uint32_t initialHeight = 900;
		utils::AppLdxDesc appDesc{};
		appDesc.instance = instance;
		appDesc.showCommand = showCommand;
		appDesc.className = L"Ldx12ImGuiNodeEditor";
		appDesc.title = L"Ldx12 - ImGui Node Editor";
		appDesc.width = initialWidth;
		appDesc.height = initialHeight;
		appDesc.messageHandler = HandleImGuiMessage;
		utils::AppLdx app( appDesc );

		GraphicsState gfx{};

		ContextDesc contextDesc{};
		contextDesc.enableDebugLayer = true;
		contextDesc.swapchainBufferCount = kFramesInFlight;
		SwapchainDesc swapchainDesc{};
		swapchainDesc.window = MakeWin32WindowHandle( app.GetWindow() );
		swapchainDesc.width = initialWidth;
		swapchainDesc.height = initialHeight;
		swapchainDesc.vsync = true;
		DeviceManager& manager = DeviceManager::Initialize( contextDesc, swapchainDesc );
		app.SetDeviceManager( manager );
		RenderDevice& device = *manager.GetRenderDevice();
		gfx.imgui = std::make_unique<App::ImGuiLayer>( app.GetWindow(), device, contextDesc.swapchainFormat, DXGI_FORMAT_UNKNOWN, kFramesInFlight );
		gfx.cpuPixels = CreateDemoPixels();
		gfx.demoTexture = UploadCpuTexture( device, gfx.cpuPixels );
		gfx.editor.imguiTexture = gfx.demoTexture;
		BuildExampleGraph( gfx.editor );

		while( app.PumpMessages() )
		{
			if( app.IsWindowMinimized() )
			{
				WaitMessage();
				continue;
			}
			if( gfx.editor.cpuInvertRequested )
				ApplyCpuInvert( gfx, device );

			device.Wait( gfx.frameSubmissions[ gfx.frameIndex ] );
			gfx.imgui->NewFrame();
			DrawPalette( gfx.editor );
			DrawEditor( gfx.editor );

			CommandBuffer& commands = device.AcquireCommandBuffer();
			const TextureHandle backBuffer = device.GetCurrentSwapchainTexture();
			RenderPass renderPass{};
			renderPass.color[ 0 ].loadOp = LoadOp::Clear;
			renderPass.color[ 0 ].clearColor = { 0.012f, 0.015f, 0.021f, 1.0f };
			Framebuffer framebuffer{};
			framebuffer.color[ 0 ].texture = backBuffer;
			commands.CmdBeginRendering( renderPass, framebuffer );
			gfx.imgui->Render( commands );
			commands.CmdEndRendering();
			gfx.frameSubmissions[ gfx.frameIndex ] = device.Submit( commands, backBuffer );
			gfx.frameIndex = ( gfx.frameIndex + 1 ) % kFramesInFlight;
		}

		manager.WaitIdle();
		gfx.editor.imguiTexture = {};
		device.Destroy( gfx.demoTexture );
		gfx.demoTexture = {};
		gfx.imgui.reset();
		DeviceManager::ShutdownSingleton();
		return 0;
	}
	catch( const std::exception& exception )
	{
		DeviceManager::ShutdownSingleton();
		MessageBoxA( nullptr, exception.what(), "ImGui Node Editor error", MB_OK | MB_ICONERROR );
		return 1;
	}
}
