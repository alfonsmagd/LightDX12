#include "RetroRenderer.hpp"
#include <Ldx12/HLSLLoader.hpp>
#include "RetroSettings.hpp"
#include <stdexcept>
namespace desktop_retro
{
	using namespace ldx12;
	namespace
	{
		struct NewPixieTargets
		{
			std::array<TextureHandle, 2> history = {};
			TextureHandle blurHorizontal = {};
			TextureHandle blurVertical = {};
			uint32_t historyReadIndex = 0;
			uint32_t width = 0;
			uint32_t height = 0;
			bool historyValid = false;
		};

		struct RendererResources
		{
			RenderPipelineState presentPipeline;
			RenderPipelineState newPixieAccumulatePipeline;
			RenderPipelineState newPixieBlurPipeline;
			NewPixieTargets newPixieTargets;
		};

		using namespace ldx12;
		RenderPipelineState CreatePipeline( RenderDevice& renderDevice, DXGI_FORMAT renderTargetFormat, const char* fragmentShaderPath )
		{
			RenderPipelineDesc desc{};
			desc.vertexShader = HLSLLoader::LoadStage( "shaders/DesktopRetroOverlayVS.hlsl", "vs_6_6" );
			desc.fragmentShader = HLSLLoader::LoadStage( fragmentShaderPath, "ps_6_6" );
			desc.color[ 0 ].format = renderTargetFormat;
			desc.rasterizerState.CullMode = D3D12_CULL_MODE_NONE;
			desc.depthStencilState.DepthEnable = FALSE;
			desc.depthStencilState.StencilEnable = FALSE;
			return renderDevice.CreateRenderPipeline( desc );
		}

		void DestroyNewPixieTargets( RenderDevice& renderDevice, NewPixieTargets& targets )
		{
			for( TextureHandle& historyTexture : targets.history )
			{
				if( historyTexture.Valid() )
				{
					renderDevice.Destroy( historyTexture );
					historyTexture = {};
				}
			}
			if( targets.blurHorizontal.Valid() )
			{
				renderDevice.Destroy( targets.blurHorizontal );
				targets.blurHorizontal = {};
			}
			if( targets.blurVertical.Valid() )
			{
				renderDevice.Destroy( targets.blurVertical );
				targets.blurVertical = {};
			}
			targets.historyReadIndex = 0;
			targets.width = 0;
			targets.height = 0;
			targets.historyValid = false;
		}

		void RecreateNewPixieTargets( RenderDevice& renderDevice, NewPixieTargets& targets, uint32_t width, uint32_t height, DXGI_FORMAT format )
		{
			if( targets.history[ 0 ].Valid() && targets.history[ 1 ].Valid() && targets.blurHorizontal.Valid() && targets.blurVertical.Valid() &&
				targets.width == width && targets.height == height )
			{
				return;
			}

			DestroyNewPixieTargets( renderDevice, targets );
			TextureDesc desc{};
			desc.width = width;
			desc.height = height;
			desc.format = format;
			desc.usage = TextureUsage::Sampled | TextureUsage::RenderTarget;
			desc.useClearValue = true;
			desc.clearValue.Format = format;
			desc.clearValue.Color[ 0 ] = 0.0f;
			desc.clearValue.Color[ 1 ] = 0.0f;
			desc.clearValue.Color[ 2 ] = 0.0f;
			desc.clearValue.Color[ 3 ] = 1.0f;

			desc.debugName = "DesktopRetroOverlay NewPixie History A";
			targets.history[ 0 ] = renderDevice.CreateTexture( desc );
			desc.debugName = "DesktopRetroOverlay NewPixie History B";
			targets.history[ 1 ] = renderDevice.CreateTexture( desc );
			desc.debugName = "DesktopRetroOverlay NewPixie Horizontal Blur";
			targets.blurHorizontal = renderDevice.CreateTexture( desc );
			desc.debugName = "DesktopRetroOverlay NewPixie Vertical Blur";
			targets.blurVertical = renderDevice.CreateTexture( desc );
			targets.width = width;
			targets.height = height;
		}

		TextureHandle RecordNewPixiePasses( RenderDevice& renderDevice,
			CommandBuffer& commandBuffer,
			RendererResources& renderer,
			TextureHandle capturedDesktop )
		{
			NewPixieTargets& targets = renderer.newPixieTargets;
			const uint32_t historyReadIndex = targets.historyReadIndex;
			const uint32_t historyWriteIndex = 1u - historyReadIndex;
			const TextureHandle historyRead = targets.history[ historyReadIndex ];
			const TextureHandle historyWrite = targets.history[ historyWriteIndex ];

			RenderPass pass{};
			pass.color[ 0 ].loadOp = LoadOp::Clear;
			pass.color[ 0 ].clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
			Framebuffer framebuffer{};

			commandBuffer.CmdTransitionTexture( capturedDesktop, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE );
			commandBuffer.CmdTransitionTexture( historyRead, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE );
			framebuffer.color[ 0 ].texture = historyWrite;
			NewPixieAccumulatePushConstants accumulateConstants{};
			accumulateConstants.sourceTextureIndex = renderDevice.GetBindlessIndex( capturedDesktop );
			accumulateConstants.historyTextureIndex = renderDevice.GetBindlessIndex( historyRead );
			accumulateConstants.persistence = targets.historyValid ? 0.38f : 0.0f;
			commandBuffer.CmdBeginRendering( pass, framebuffer );
			commandBuffer.CmdBindRenderPipeline( renderer.newPixieAccumulatePipeline );
			commandBuffer.CmdPushConstants( &accumulateConstants, sizeof( accumulateConstants ) );
			commandBuffer.CmdDraw( 3 );
			commandBuffer.CmdEndRendering();
			commandBuffer.CmdTransitionTexture( capturedDesktop, D3D12_RESOURCE_STATE_COMMON );
			commandBuffer.CmdTransitionTexture( historyWrite, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE );

			NewPixieBlurPushConstants blurConstants{};
			blurConstants.sourceTextureIndex = renderDevice.GetBindlessIndex( historyWrite );
			blurConstants.stepX = 0.85f / static_cast<float>( targets.width );
			framebuffer.color[ 0 ].texture = targets.blurHorizontal;
			commandBuffer.CmdBeginRendering( pass, framebuffer );
			commandBuffer.CmdBindRenderPipeline( renderer.newPixieBlurPipeline );
			commandBuffer.CmdPushConstants( &blurConstants, sizeof( blurConstants ) );
			commandBuffer.CmdDraw( 3 );
			commandBuffer.CmdEndRendering();
			commandBuffer.CmdTransitionTexture( targets.blurHorizontal, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE );

			blurConstants.sourceTextureIndex = renderDevice.GetBindlessIndex( targets.blurHorizontal );
			blurConstants.stepX = 0.0f;
			blurConstants.stepY = 0.85f / static_cast<float>( targets.height );
			framebuffer.color[ 0 ].texture = targets.blurVertical;
			commandBuffer.CmdBeginRendering( pass, framebuffer );
			commandBuffer.CmdBindRenderPipeline( renderer.newPixieBlurPipeline );
			commandBuffer.CmdPushConstants( &blurConstants, sizeof( blurConstants ) );
			commandBuffer.CmdDraw( 3 );
			commandBuffer.CmdEndRendering();
			commandBuffer.CmdTransitionTexture( targets.blurVertical, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE );

			targets.historyReadIndex = historyWriteIndex;
			targets.historyValid = true;
			return historyWrite;
		}

		void RecordFrame( RenderDevice& renderDevice,
			CommandBuffer& commandBuffer,
			TextureHandle currentBackbuffer,
			RendererResources& renderer,
			TextureHandle capturedDesktop,
			RetroPushConstants settings )
		{
			const bool newPixieMode = settings.mode == 4;
			const TextureHandle processedDesktop =
				newPixieMode ? RecordNewPixiePasses( renderDevice, commandBuffer, renderer, capturedDesktop ) : capturedDesktop;

			RenderPass pass{};
			pass.color[ 0 ].loadOp = LoadOp::Clear;
			pass.color[ 0 ].clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
			Framebuffer framebuffer{};
			framebuffer.color[ 0 ].texture = currentBackbuffer;

			RetroPushConstants pushConstants = settings;
			pushConstants.sourceTextureIndex = renderDevice.GetBindlessIndex( processedDesktop );
			pushConstants.blurTextureIndex =
				newPixieMode ? renderDevice.GetBindlessIndex( renderer.newPixieTargets.blurVertical ) : pushConstants.sourceTextureIndex;

			if( !newPixieMode )
			{
				commandBuffer.CmdTransitionTexture( capturedDesktop, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE );
			}
			commandBuffer.CmdBeginRendering( pass, framebuffer );
			commandBuffer.CmdBindRenderPipeline( renderer.presentPipeline );
			commandBuffer.CmdPushConstants( &pushConstants, sizeof( pushConstants ) );
			commandBuffer.CmdDraw( 3 );
			commandBuffer.CmdEndRendering();
			if( !newPixieMode )
			{
				// COMMON is the required ownership hand-off state before D3D11 writes the next frame.
				commandBuffer.CmdTransitionTexture( capturedDesktop, D3D12_RESOURCE_STATE_COMMON );
			}
		}
	} // internal passes
	struct RetroRenderer::Impl
	{
		RenderDevice& device;
		DXGI_FORMAT format;
		RendererResources resources;
		uint32_t width = 0, height = 0;
		bool multipassInitialized = false;
		RetroPreset lastPreset = RetroPreset::Ps2Clean;
		Impl( RenderDevice& d, DXGI_FORMAT f ) : device( d ), format( f )
		{
			resources.presentPipeline = CreatePipeline( d, f, "shaders/DesktopRetroOverlayPS.hlsl" );
		}
		~Impl()
		{
			try
			{
				device.WaitIdle();
			}
			catch( ... )
			{
			}
			try
			{
				DestroyNewPixieTargets( device, resources.newPixieTargets );
			}
			catch( ... )
			{
			}
		}
	};
	RetroRenderer::RetroRenderer( RenderDevice& device, DXGI_FORMAT format ) : impl_( std::make_unique<Impl>( device, format ) )
	{
	}
	RetroRenderer::~RetroRenderer() = default;
	void RetroRenderer::ResetHistory() noexcept
	{
		impl_->resources.newPixieTargets.historyValid = false;
	}
	void RetroRenderer::Record( CommandBuffer& commands, TextureHandle output, const CapturedFrame& frame, const EffectSettings& settings, float time )
	{
		if( !frame.texture.Valid() || !output.Valid() || frame.texture == output || !frame.width || !frame.height )
			throw std::invalid_argument( "Renderer requires a valid captured frame and dimensions." );
		auto& state = *impl_;
		if( state.lastPreset != settings.preset || state.width != frame.width || state.height != frame.height )
		{
			ResetHistory();
			state.lastPreset = settings.preset;
			state.width = frame.width;
			state.height = frame.height;
		}
		if( settings.preset == RetroPreset::NewPixie )
		{
			// Compile and allocate multipass resources only when a preset needs them.
			if( !state.multipassInitialized )
			{
				state.resources.newPixieAccumulatePipeline = CreatePipeline( state.device, state.format, "shaders/NewPixieAccumulatePS.hlsl" );
				state.resources.newPixieBlurPipeline = CreatePipeline( state.device, state.format, "shaders/NewPixieBlurPS.hlsl" );
				state.multipassInitialized = true;
			}
			RecreateNewPixieTargets( state.device, state.resources.newPixieTargets, frame.width, frame.height, state.format );
		}
		RetroPushConstants constants{};
		constants.mode = static_cast<uint32_t>( settings.preset );
		constants.intensity = settings.intensity;
		constants.curvature = settings.curvature;
		constants.scanlineStrength = settings.scanlineStrength;
		constants.chromaticAberrationPixels = settings.chromaticAberrationPixels;
		constants.noiseStrength = settings.noiseStrength;
		constants.time = time;
		constants.inverseWidth = 1.f / static_cast<float>( frame.width );
		constants.inverseHeight = 1.f / static_cast<float>( frame.height );
		RecordFrame( state.device, commands, output, state.resources, frame.texture, constants );
	}
}
