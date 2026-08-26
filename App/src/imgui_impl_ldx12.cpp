#include "App/imgui_impl_ldx12.h"

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <new>

namespace
{
	constexpr uint32_t ourMaxFramesInFlight = 3;
	constexpr uint32_t ourInitialVertexCapacity = 5000;
	constexpr uint32_t ourInitialIndexCapacity = 10000;

	struct ImGui_ImplLdx12_FrameResources final
	{
		ldx12::BufferHandle vertexBuffer{};
		ldx12::BufferHandle indexBuffer{};
		uint32_t vertexCapacity = 0;
		uint32_t indexCapacity = 0;
	};

	struct ImGui_ImplLdx12_Data final
	{
		ldx12::RenderDevice* device = nullptr;
		ldx12::RenderPipelineState pipeline{};
		ldx12::TextureHandle fontTexture{};
		std::array<ImGui_ImplLdx12_FrameResources, ourMaxFramesInFlight> frames{};
		uint32_t framesInFlight = 0;
		uint32_t frameIndex = 0;
	};

	struct ImGui_ImplLdx12_PushConstants final
	{
		float scale[ 2 ] = {};
		float translate[ 2 ] = {};
		uint32_t textureIndex = 0;
	};

	ImGui_ImplLdx12_Data* ImGui_ImplLdx12_GetBackendData() noexcept
	{
		if( ImGui::GetCurrentContext() == nullptr )
		{
			return nullptr;
		}
		return static_cast<ImGui_ImplLdx12_Data*>( ImGui::GetIO().BackendRendererUserData );
	}

	ldx12::RenderPipelineState ImGui_ImplLdx12_CreatePipeline(
		ldx12::RenderDevice& device,
		DXGI_FORMAT renderTargetFormat,
		DXGI_FORMAT depthFormat )
	{
		static constexpr char ourVertexShader[] = R"(
cbuffer PushConstants : register(b0)
{
    float2 gScale;
    float2 gTranslate;
    uint gTextureIndex;
};

struct VertexInput
{
    float2 position : POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

struct PixelInput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

PixelInput VSMain(VertexInput input)
{
    PixelInput output;
    output.position = float4(input.position * gScale + gTranslate, 0.0, 1.0);
    output.uv = input.uv;
    output.color = input.color;
    return output;
}
)";

		static constexpr char ourPixelShader[] = R"(
cbuffer PushConstants : register(b0)
{
    float2 gScale;
    float2 gTranslate;
    uint gTextureIndex;
};

SamplerState gLinearClampSampler : register(s0);

struct PixelInput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

float4 PSMain(PixelInput input) : SV_Target0
{
    Texture2D<float4> textureResource = ResourceDescriptorHeap[gTextureIndex];
    return input.color * textureResource.Sample(gLinearClampSampler, input.uv);
}
)";

		ldx12::RenderPipelineDesc desc{};
		desc.vertexShader.source = ourVertexShader;
		desc.vertexShader.entryPoint = "VSMain";
		desc.vertexShader.profile = "vs_6_6";
		desc.vertexShader.sourceName = "imgui_impl_ldx12_vs";
		desc.fragmentShader.source = ourPixelShader;
		desc.fragmentShader.entryPoint = "PSMain";
		desc.fragmentShader.profile = "ps_6_6";
		desc.fragmentShader.sourceName = "imgui_impl_ldx12_ps";
		desc.color[ 0 ].format = renderTargetFormat;
		desc.colorFormat = DXGI_FORMAT_UNKNOWN;
		desc.depthFormat = depthFormat;

		desc.inputElements[ 0 ].semanticName = "POSITION";
		desc.inputElements[ 0 ].format = DXGI_FORMAT_R32G32_FLOAT;
		desc.inputElements[ 0 ].alignedByteOffset = static_cast<uint32_t>( offsetof( ImDrawVert, pos ) );
		desc.inputElements[ 1 ].semanticName = "TEXCOORD";
		desc.inputElements[ 1 ].format = DXGI_FORMAT_R32G32_FLOAT;
		desc.inputElements[ 1 ].alignedByteOffset = static_cast<uint32_t>( offsetof( ImDrawVert, uv ) );
		desc.inputElements[ 2 ].semanticName = "COLOR";
		desc.inputElements[ 2 ].format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.inputElements[ 2 ].alignedByteOffset = static_cast<uint32_t>( offsetof( ImDrawVert, col ) );

		desc.blendState.AlphaToCoverageEnable = FALSE;
		desc.blendState.IndependentBlendEnable = FALSE;
		D3D12_RENDER_TARGET_BLEND_DESC& blend = desc.blendState.RenderTarget[ 0 ];
		blend.BlendEnable = TRUE;
		blend.LogicOpEnable = FALSE;
		blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
		blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		blend.BlendOp = D3D12_BLEND_OP_ADD;
		blend.SrcBlendAlpha = D3D12_BLEND_ONE;
		blend.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
		blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
		blend.LogicOp = D3D12_LOGIC_OP_NOOP;
		blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

		desc.rasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
		desc.rasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		desc.rasterizerState.DepthClipEnable = TRUE;
		desc.depthStencilState.DepthEnable = FALSE;
		desc.depthStencilState.StencilEnable = FALSE;
		return device.CreateRenderPipeline( desc );
	}

	ldx12::TextureHandle ImGui_ImplLdx12_CreateFontTexture( ldx12::RenderDevice& device )
	{
		ImGuiIO& io = ImGui::GetIO();
		unsigned char* pixels = nullptr;
		int width = 0;
		int height = 0;
		int bytesPerPixel = 0;
		io.Fonts->GetTexDataAsRGBA32( &pixels, &width, &height, &bytesPerPixel );
		if( pixels == nullptr || width <= 0 || height <= 0 || bytesPerPixel != 4 )
		{
			return {};
		}

		ldx12::TextureDesc desc{};
		desc.debugName = "Dear ImGui font atlas";
		desc.width = static_cast<uint32_t>( width );
		desc.height = static_cast<uint32_t>( height );
		desc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.usage = ldx12::TextureUsage::Sampled;
		desc.initialState = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
		desc.data = pixels;
		desc.rowPitch = static_cast<uint32_t>( width * bytesPerPixel );
		desc.slicePitch = desc.rowPitch * static_cast<uint32_t>( height );
		return device.CreateTexture( desc );
	}

	void ImGui_ImplLdx12_DestroyFrameResources(
		ldx12::RenderDevice& device,
		ImGui_ImplLdx12_FrameResources& frame )
	{
		if( frame.vertexBuffer.Valid() )
		{
			device.Destroy( frame.vertexBuffer );
			frame.vertexBuffer = {};
		}
		if( frame.indexBuffer.Valid() )
		{
			device.Destroy( frame.indexBuffer );
			frame.indexBuffer = {};
		}
		frame.vertexCapacity = 0;
		frame.indexCapacity = 0;
	}

	bool ImGui_ImplLdx12_EnsureFrameResources(
		ImGui_ImplLdx12_Data& data,
		ImGui_ImplLdx12_FrameResources& frame,
		uint32_t vertexCount,
		uint32_t indexCount )
	{
		if( frame.vertexCapacity < vertexCount )
		{
			if( frame.vertexBuffer.Valid() )
			{
				data.device->Destroy( frame.vertexBuffer );
				frame.vertexBuffer = {};
			}
			frame.vertexCapacity = vertexCount + ourInitialVertexCapacity;
			ldx12::BufferDesc desc{};
			desc.debugName = "Dear ImGui vertex buffer";
			desc.size = static_cast<uint64_t>( frame.vertexCapacity ) * sizeof( ImDrawVert );
			desc.stride = sizeof( ImDrawVert );
			desc.bufferType = ldx12::BufferDesc::BufferType::VertexBuffer;
			desc.heapType = D3D12_HEAP_TYPE_UPLOAD;
			frame.vertexBuffer = data.device->CreateBuffer( desc );
		}

		if( frame.indexCapacity < indexCount )
		{
			if( frame.indexBuffer.Valid() )
			{
				data.device->Destroy( frame.indexBuffer );
				frame.indexBuffer = {};
			}
			frame.indexCapacity = indexCount + ourInitialIndexCapacity;
			ldx12::BufferDesc desc{};
			desc.debugName = "Dear ImGui index buffer";
			desc.size = static_cast<uint64_t>( frame.indexCapacity ) * sizeof( ImDrawIdx );
			desc.stride = sizeof( ImDrawIdx );
			desc.bufferType = ldx12::BufferDesc::BufferType::IndexBuffer;
			desc.heapType = D3D12_HEAP_TYPE_UPLOAD;
			frame.indexBuffer = data.device->CreateBuffer( desc );
		}

		return frame.vertexBuffer.Valid() && frame.indexBuffer.Valid();
	}

	void ImGui_ImplLdx12_UploadDrawData(
		ldx12::RenderDevice& device,
		const ImDrawData& drawData,
		const ImGui_ImplLdx12_FrameResources& frame )
	{
		uint64_t vertexOffset = 0;
		uint64_t indexOffset = 0;
		for( int listIndex = 0; listIndex < drawData.CmdListsCount; ++listIndex )
		{
			const ImDrawList* drawList = drawData.CmdLists[ listIndex ];
			const uint64_t vertexBytes = static_cast<uint64_t>( drawList->VtxBuffer.Size ) * sizeof( ImDrawVert );
			const uint64_t indexBytes = static_cast<uint64_t>( drawList->IdxBuffer.Size ) * sizeof( ImDrawIdx );
			device.WriteBuffer( frame.vertexBuffer, vertexOffset, drawList->VtxBuffer.Data, vertexBytes );
			device.WriteBuffer( frame.indexBuffer, indexOffset, drawList->IdxBuffer.Data, indexBytes );
			vertexOffset += vertexBytes;
			indexOffset += indexBytes;
		}
	}

	void ImGui_ImplLdx12_SetupRenderState(
		const ImDrawData& drawData,
		const ImGui_ImplLdx12_Data& data,
		const ImGui_ImplLdx12_FrameResources& frame,
		ldx12::ICommandBuffer& commandBuffer )
	{
		commandBuffer.CmdSetViewport(
			0.0f,
			0.0f,
			drawData.DisplaySize.x * drawData.FramebufferScale.x,
			drawData.DisplaySize.y * drawData.FramebufferScale.y );
		commandBuffer.CmdBindRenderPipeline( data.pipeline );
		commandBuffer.CmdBindVertexBuffer( frame.vertexBuffer, sizeof( ImDrawVert ) );
		commandBuffer.CmdBindIndexBuffer(
			frame.indexBuffer,
			sizeof( ImDrawIdx ) == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT );
	}
}

bool ImGui_ImplLdx12_Init( const ImGui_ImplLdx12_InitInfo& info )
{
	ImGuiIO& io = ImGui::GetIO();
	assert( io.BackendRendererUserData == nullptr );
	if( info.device == nullptr ||
		info.renderTargetFormat == DXGI_FORMAT_UNKNOWN ||
		info.framesInFlight == 0 ||
		info.framesInFlight > ourMaxFramesInFlight ||
		io.BackendRendererUserData != nullptr )
	{
		return false;
	}

	ImGui_ImplLdx12_Data* data = new( std::nothrow ) ImGui_ImplLdx12_Data{};
	if( data == nullptr )
	{
		return false;
	}

	data->device = info.device;
	data->framesInFlight = info.framesInFlight;
	try
	{
		data->pipeline = ImGui_ImplLdx12_CreatePipeline(
			*data->device,
			info.renderTargetFormat,
			info.depthFormat );
		data->fontTexture = ImGui_ImplLdx12_CreateFontTexture( *data->device );
		if( !data->pipeline.Valid() || !data->fontTexture.Valid() )
		{
			throw std::bad_alloc{};
		}
	}
	catch( ... )
	{
		if( data->fontTexture.Valid() )
		{
			data->device->Destroy( data->fontTexture );
		}
		delete data;
		return false;
	}

	io.BackendRendererUserData = data;
	io.BackendRendererName = "imgui_impl_ldx12";
	io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
	io.Fonts->SetTexID( static_cast<ImTextureID>( data->device->GetBindlessIndex( data->fontTexture ) ) );
	return true;
}

void ImGui_ImplLdx12_Shutdown()
{
	ImGui_ImplLdx12_Data* data = ImGui_ImplLdx12_GetBackendData();
	assert( data != nullptr );
	if( data == nullptr )
	{
		return;
	}

	ImGuiIO& io = ImGui::GetIO();
	data->device->WaitIdle();
	for( ImGui_ImplLdx12_FrameResources& frame : data->frames )
	{
		ImGui_ImplLdx12_DestroyFrameResources( *data->device, frame );
	}
	if( data->fontTexture.Valid() )
	{
		data->device->Destroy( data->fontTexture );
		data->fontTexture = {};
	}
	data->pipeline = {};
	io.Fonts->SetTexID( ImTextureID_Invalid );
	io.BackendRendererName = nullptr;
	io.BackendRendererUserData = nullptr;
	io.BackendFlags &= ~ImGuiBackendFlags_RendererHasVtxOffset;
	delete data;
}

void ImGui_ImplLdx12_NewFrame()
{
	assert( ImGui_ImplLdx12_GetBackendData() != nullptr );
}

void ImGui_ImplLdx12_RenderDrawData( ImDrawData* drawData, ldx12::ICommandBuffer& commandBuffer )
{
	ImGui_ImplLdx12_Data* data = ImGui_ImplLdx12_GetBackendData();
	assert( data != nullptr );
	if( data == nullptr || drawData == nullptr )
	{
		return;
	}
	ImGui_ImplLdx12_FrameResources& frame = data->frames[ data->frameIndex ];
	data->frameIndex = ( data->frameIndex + 1u ) % data->framesInFlight;

	const float framebufferWidth = drawData->DisplaySize.x * drawData->FramebufferScale.x;
	const float framebufferHeight = drawData->DisplaySize.y * drawData->FramebufferScale.y;
	if( framebufferWidth <= 0.0f || framebufferHeight <= 0.0f || drawData->CmdListsCount == 0 )
	{
		return;
	}

	const uint32_t vertexCount = static_cast<uint32_t>( drawData->TotalVtxCount );
	const uint32_t indexCount = static_cast<uint32_t>( drawData->TotalIdxCount );
	if( !ImGui_ImplLdx12_EnsureFrameResources( *data, frame, vertexCount, indexCount ) )
	{
		return;
	}
	ImGui_ImplLdx12_UploadDrawData( *data->device, *drawData, frame );
	ImGui_ImplLdx12_SetupRenderState( *drawData, *data, frame, commandBuffer );

	const ImVec2 clipOffset = drawData->DisplayPos;
	const ImVec2 clipScale = drawData->FramebufferScale;
	const float positionScaleX = 2.0f / drawData->DisplaySize.x;
	const float positionScaleY = -2.0f / drawData->DisplaySize.y;
	uint32_t globalVertexOffset = 0;
	uint32_t globalIndexOffset = 0;

	for( int listIndex = 0; listIndex < drawData->CmdListsCount; ++listIndex )
	{
		const ImDrawList* drawList = drawData->CmdLists[ listIndex ];
		for( int commandIndex = 0; commandIndex < drawList->CmdBuffer.Size; ++commandIndex )
		{
			const ImDrawCmd& drawCommand = drawList->CmdBuffer[ commandIndex ];
			if( drawCommand.UserCallback != nullptr )
			{
				if( drawCommand.UserCallback == ImDrawCallback_ResetRenderState )
				{
					ImGui_ImplLdx12_SetupRenderState( *drawData, *data, frame, commandBuffer );
				}
				else
				{
					drawCommand.UserCallback( drawList, &drawCommand );
				}
				continue;
			}

			ImVec2 clipMinimum(
				( drawCommand.ClipRect.x - clipOffset.x ) * clipScale.x,
				( drawCommand.ClipRect.y - clipOffset.y ) * clipScale.y );
			ImVec2 clipMaximum(
				( drawCommand.ClipRect.z - clipOffset.x ) * clipScale.x,
				( drawCommand.ClipRect.w - clipOffset.y ) * clipScale.y );
			if( clipMinimum.x < 0.0f ) clipMinimum.x = 0.0f;
			if( clipMinimum.y < 0.0f ) clipMinimum.y = 0.0f;
			if( clipMaximum.x > framebufferWidth ) clipMaximum.x = framebufferWidth;
			if( clipMaximum.y > framebufferHeight ) clipMaximum.y = framebufferHeight;
			if( clipMaximum.x <= clipMinimum.x || clipMaximum.y <= clipMinimum.y )
			{
				continue;
			}

			const ImTextureID textureId = drawCommand.GetTexID();
			assert( textureId != ImTextureID_Invalid );
			assert( textureId <= std::numeric_limits<uint32_t>::max() );
			if( textureId == ImTextureID_Invalid || textureId > std::numeric_limits<uint32_t>::max() )
			{
				continue;
			}

			ImGui_ImplLdx12_PushConstants constants{};
			constants.scale[ 0 ] = positionScaleX;
			constants.scale[ 1 ] = positionScaleY;
			constants.translate[ 0 ] = -1.0f - drawData->DisplayPos.x * positionScaleX;
			constants.translate[ 1 ] = 1.0f - drawData->DisplayPos.y * positionScaleY;
			constants.textureIndex = static_cast<uint32_t>( textureId );
			commandBuffer.CmdPushConstants( &constants, sizeof( constants ) );
			commandBuffer.CmdSetScissor(
				static_cast<int32_t>( clipMinimum.x ),
				static_cast<int32_t>( clipMinimum.y ),
				static_cast<int32_t>( clipMaximum.x ),
				static_cast<int32_t>( clipMaximum.y ) );
			commandBuffer.CmdDrawIndexed(
				drawCommand.ElemCount,
				1,
				globalIndexOffset + drawCommand.IdxOffset,
				static_cast<int32_t>( globalVertexOffset + drawCommand.VtxOffset ) );
		}
		globalIndexOffset += static_cast<uint32_t>( drawList->IdxBuffer.Size );
		globalVertexOffset += static_cast<uint32_t>( drawList->VtxBuffer.Size );
	}
}

ImTextureRef ImGui_ImplLdx12_Texture( ldx12::TextureHandle texture )
{
	ImGui_ImplLdx12_Data* data = ImGui_ImplLdx12_GetBackendData();
	assert( data != nullptr );
	assert( data == nullptr || data->device->IsAlive( texture ) );
	if( data == nullptr || !data->device->IsAlive( texture ) )
	{
		return {};
	}
	return ImTextureRef( static_cast<ImTextureID>( data->device->GetBindlessIndex( texture ) ) );
}

void ImGui::Image(
	ldx12::TextureHandle texture,
	const ImVec2& imageSize,
	const ImVec2& uv0,
	const ImVec2& uv1 )
{
	ImGui::Image( ImGui_ImplLdx12_Texture( texture ), imageSize, uv0, uv1 );
}
