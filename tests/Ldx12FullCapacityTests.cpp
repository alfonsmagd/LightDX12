#include "Ldx12/TestTemplate.hpp"

#include <array>
#include <chrono>
#include <exception>
#include <iostream>

namespace
{
	using namespace ldx12;
	using namespace ldx12::tests;

	constexpr uint32_t ourExpectedLiveBuffers = 4'096;
	constexpr uint32_t ourExpectedLiveTextures = 4'096;
	constexpr uint32_t ourExpectedLiveSwapchains = 16;
	constexpr uint32_t ourExpectedMinimumBackbuffers = 2;
	constexpr uint32_t ourExpectedMaximumBackbuffers = 3;
	constexpr uint32_t ourExpectedColorAttachments = 8;
	constexpr uint32_t ourExpectedVertexInputElements = 16;
	constexpr uint32_t ourExpectedActiveCommandBuffers = 64;
	constexpr uint32_t ourExpectedCommandBufferBatch = 4;
	constexpr uint32_t ourExpectedTrackedTextures = 256;
	constexpr uint32_t ourExpectedPushConstantValues = 63;
	constexpr uint32_t ourTestWindowCount = ourExpectedLiveSwapchains + 1;

	LRESULT CALLBACK TestWindowProc( HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam )
	{
		return DefWindowProcW( hwnd, message, wParam, lParam );
	}

	class TestWindows final
	{
	public:
		TestWindows()
		{
			instance_ = GetModuleHandleW( nullptr );

			WNDCLASSEXW windowClass{};
			windowClass.cbSize = sizeof( windowClass );
			windowClass.lpfnWndProc = TestWindowProc;
			windowClass.hInstance = instance_;
			windowClass.lpszClassName = className_;
			if( RegisterClassExW( &windowClass ) == 0 )
			{
				throw std::runtime_error( "Full-capacity test failed to register its window class." );
			}

			for( HWND& window : windows_ )
			{
				window = CreateWindowExW( 0,
					className_,
					L"Ldx12 Full Capacity Test",
					WS_OVERLAPPEDWINDOW,
					CW_USEDEFAULT,
					CW_USEDEFAULT,
					64,
					64,
					nullptr,
					nullptr,
					instance_,
					nullptr );
				if( window == nullptr )
				{
					throw std::runtime_error( "Full-capacity test failed to create a Win32 window." );
				}
			}
		}

		~TestWindows()
		{
			for( HWND window : windows_ )
			{
				if( window != nullptr )
				{
					DestroyWindow( window );
				}
			}
			UnregisterClassW( className_, instance_ );
		}

		HWND Get( uint32_t index ) const noexcept
		{
			return windows_[ index ];
		}

	private:
		static constexpr const wchar_t* className_ = L"Ldx12FullCapacityTestWindow";
		HINSTANCE instance_ = nullptr;
		std::array<HWND, ourTestWindowCount> windows_ = {};
	};

	ContextDesc CreateSmallContext()
	{
		ContextDesc context{};
		context.enableDebugLayer = false;
		context.preferHighPerformanceAdapter = false;
		context.allowTearing = false;
		context.bindlessCapacity = LDX12_BINDLESS_DYNAMIC_SLOT_FIRST + 8u;
		context.rtvCapacity = ourExpectedColorAttachments;
		context.dsvCapacity = 1;
		return context;
	}

	void TestLiveBufferAndTextureCapacities()
	{
		ContextDesc context = CreateSmallContext();
		DeviceManagerGuard guard;
		DeviceManager& manager = DeviceManager::Initialize( context );
		guard.active = true;
		RenderDevice& device = *manager.GetRenderDevice();

		BufferDesc bufferDesc{};
		bufferDesc.debugName = "Full-capacity buffer";
		bufferDesc.size = sizeof( uint32_t );

		std::array<BufferHandle, ourExpectedLiveBuffers> buffers{};
		for( BufferHandle& buffer : buffers )
		{
			buffer = device.CreateBuffer( bufferDesc );
			Require( buffer.Valid(), "A buffer below the documented live-buffer limit was invalid." );
		}

		bool bufferLimitReached = false;
		try
		{
			const BufferHandle overflowBuffer = device.CreateBuffer( bufferDesc );
			device.Destroy( overflowBuffer );
		}
		catch( const std::length_error& )
		{
			bufferLimitReached = true;
		}
		Require( bufferLimitReached, "Ldx12 accepted more than 4,096 live buffers." );

		for( BufferHandle buffer : buffers )
		{
			Require( device.Destroy( buffer ), "Full-capacity buffer cleanup failed." );
		}

		TextureDesc textureDesc{};
		textureDesc.debugName = "Full-capacity texture";
		textureDesc.width = 1;
		textureDesc.height = 1;
		textureDesc.format = DXGI_FORMAT_R8_UNORM;
		textureDesc.usage = TextureUsage::None;

		std::array<TextureHandle, ourExpectedLiveTextures> textures{};
		for( TextureHandle& texture : textures )
		{
			texture = device.CreateTexture( textureDesc );
			Require( texture.Valid(), "A texture below the documented live-texture limit was invalid." );
		}

		bool textureLimitReached = false;
		try
		{
			const TextureHandle overflowTexture = device.CreateTexture( textureDesc );
			device.Destroy( overflowTexture );
		}
		catch( const std::length_error& )
		{
			textureLimitReached = true;
		}
		Require( textureLimitReached, "Ldx12 accepted more than 4,096 live textures." );

		for( TextureHandle texture : textures )
		{
			Require( device.Destroy( texture ), "Full-capacity texture cleanup failed." );
		}
		device.WaitIdle();
	}

	RenderPipelineState CreateEightAttachmentPipeline( RenderDevice& device )
	{
		static constexpr char vertexShader[] = R"(
float4 VSMain(uint vertexID : SV_VertexID) : SV_Position
{
    const float2 positions[3] = { float2(0.0, 0.5), float2(0.5, -0.5), float2(-0.5, -0.5) };
    return float4(positions[vertexID], 0.0, 1.0);
}
)";

		static constexpr char pixelShader[] = R"(
struct PSOutput
{
    float4 color0 : SV_Target0;
    float4 color1 : SV_Target1;
    float4 color2 : SV_Target2;
    float4 color3 : SV_Target3;
    float4 color4 : SV_Target4;
    float4 color5 : SV_Target5;
    float4 color6 : SV_Target6;
    float4 color7 : SV_Target7;
};

PSOutput PSMain()
{
    PSOutput output;
    output.color0 = float4(1.0, 0.0, 0.0, 1.0);
    output.color1 = float4(0.0, 1.0, 0.0, 1.0);
    output.color2 = float4(0.0, 0.0, 1.0, 1.0);
    output.color3 = float4(1.0, 1.0, 0.0, 1.0);
    output.color4 = float4(1.0, 0.0, 1.0, 1.0);
    output.color5 = float4(0.0, 1.0, 1.0, 1.0);
    output.color6 = float4(0.5, 0.5, 0.5, 1.0);
    output.color7 = float4(1.0, 1.0, 1.0, 1.0);
    return output;
}
)";

		RenderPipelineDesc desc{};
		desc.vertexShader.source = vertexShader;
		desc.vertexShader.entryPoint = "VSMain";
		desc.vertexShader.profile = "vs_6_6";
		desc.fragmentShader.source = pixelShader;
		desc.fragmentShader.entryPoint = "PSMain";
		desc.fragmentShader.profile = "ps_6_6";
		desc.colorFormat = DXGI_FORMAT_UNKNOWN;
		for( RenderPipelineColorAttachmentDesc& color : desc.color )
		{
			color.format = DXGI_FORMAT_R8G8B8A8_UNORM;
		}
		desc.depthStencilState.DepthEnable = FALSE;
		desc.depthStencilState.StencilEnable = FALSE;
		return device.CreateRenderPipeline( desc );
	}

	RenderPipelineState CreateSixteenInputPipeline( RenderDevice& device )
	{
		static constexpr char vertexShader[] = R"(
struct VSInput
{
    float value0 : ATTRIBUTE0;
    float value1 : ATTRIBUTE1;
    float value2 : ATTRIBUTE2;
    float value3 : ATTRIBUTE3;
    float value4 : ATTRIBUTE4;
    float value5 : ATTRIBUTE5;
    float value6 : ATTRIBUTE6;
    float value7 : ATTRIBUTE7;
    float value8 : ATTRIBUTE8;
    float value9 : ATTRIBUTE9;
    float value10 : ATTRIBUTE10;
    float value11 : ATTRIBUTE11;
    float value12 : ATTRIBUTE12;
    float value13 : ATTRIBUTE13;
    float value14 : ATTRIBUTE14;
    float value15 : ATTRIBUTE15;
};

float4 VSMain(VSInput input) : SV_Position
{
    const float total = input.value0 + input.value1 + input.value2 + input.value3 +
        input.value4 + input.value5 + input.value6 + input.value7 +
        input.value8 + input.value9 + input.value10 + input.value11 +
        input.value12 + input.value13 + input.value14 + input.value15;
    return float4(total, 0.0, 0.0, 1.0);
}
)";

		static constexpr char pixelShader[] = R"(
float4 PSMain() : SV_Target0
{
    return float4(1.0, 1.0, 1.0, 1.0);
}
)";

		RenderPipelineDesc desc{};
		desc.vertexShader.source = vertexShader;
		desc.vertexShader.entryPoint = "VSMain";
		desc.vertexShader.profile = "vs_6_6";
		desc.fragmentShader.source = pixelShader;
		desc.fragmentShader.entryPoint = "PSMain";
		desc.fragmentShader.profile = "ps_6_6";
		desc.depthStencilState.DepthEnable = FALSE;
		desc.depthStencilState.StencilEnable = FALSE;
		for( uint32_t index = 0; index < desc.inputElements.size(); ++index )
		{
			VertexInputElementDesc& element = desc.inputElements[ index ];
			element.semanticName = "ATTRIBUTE";
			element.semanticIndex = index;
			element.format = DXGI_FORMAT_R32_FLOAT;
			element.inputSlot = 0;
			element.alignedByteOffset = index * sizeof( float );
		}
		return device.CreateRenderPipeline( desc );
	}

	void TestPipelineAndRenderPassCapacities( RenderDevice& device )
	{
		Require( ourMaxColorAttachments == ourExpectedColorAttachments, "The public color-attachment capacity is not eight." );
		Require( ourMaxVertexInputElements == ourExpectedVertexInputElements, "The public vertex-input capacity is not 16." );

		RenderPipelineState inputPipeline = CreateSixteenInputPipeline( device );
		Require( inputPipeline.Valid(), "A pipeline with 16 vertex input elements was not created." );

		RenderPipelineState attachmentPipeline = CreateEightAttachmentPipeline( device );
		Require( attachmentPipeline.Valid(), "A pipeline with eight color outputs was not created." );

		TextureDesc targetDesc{};
		targetDesc.debugName = "Full-capacity color attachment";
		targetDesc.width = 4;
		targetDesc.height = 4;
		targetDesc.usage = TextureUsage::RenderTarget;

		std::array<TextureHandle, ourExpectedColorAttachments> targets{};
		RenderPass renderPass{};
		Framebuffer framebuffer{};
		for( uint32_t index = 0; index < targets.size(); ++index )
		{
			targets[ index ] = device.CreateTexture( targetDesc );
			renderPass.color[ index ].loadOp = LoadOp::Clear;
			framebuffer.color[ index ].texture = targets[ index ];
		}

		ICommandBuffer& commands = device.AcquireCommandBuffer();
		commands.CmdBeginRendering( renderPass, framebuffer );
		commands.CmdBindRenderPipeline( attachmentPipeline );
		commands.CmdDraw( 3 );
		commands.CmdEndRendering();
		const SubmitHandle submission = device.Submit( commands );
		device.Wait( submission );

		for( TextureHandle target : targets )
		{
			Require( device.Destroy( target ), "Eight-attachment test cleanup failed." );
		}
	}

	void TestTextureTrackingCapacity( RenderDevice& device )
	{
		Require( ourMaxTrackedTexturesPerCommandBuffer == ourExpectedTrackedTextures, "The public tracked-texture capacity is not 256." );

		TextureDesc textureDesc{};
		textureDesc.debugName = "Full-capacity tracked texture";
		textureDesc.width = 1;
		textureDesc.height = 1;
		textureDesc.format = DXGI_FORMAT_R8_UNORM;
		textureDesc.usage = TextureUsage::None;

		std::array<TextureHandle, ourExpectedTrackedTextures + 1u> textures{};
		for( TextureHandle& texture : textures )
		{
			texture = device.CreateTexture( textureDesc );
		}

		ICommandBuffer& commands = device.AcquireCommandBuffer();
		for( uint32_t index = 0; index < ourExpectedTrackedTextures; ++index )
		{
			commands.CmdTransitionTexture( textures[ index ], D3D12_RESOURCE_STATE_COPY_DEST );
		}

		bool trackingLimitReached = false;
		try
		{
			commands.CmdTransitionTexture( textures.back(), D3D12_RESOURCE_STATE_COPY_DEST );
		}
		catch( const std::length_error& )
		{
			trackingLimitReached = true;
		}
		Require( trackingLimitReached, "A command buffer tracked more than 256 textures." );

		const SubmitHandle submission = device.Submit( commands );
		device.Wait( submission );
		for( TextureHandle texture : textures )
		{
			Require( device.Destroy( texture ), "Tracked-texture test cleanup failed." );
		}
	}

	void TestPushConstantCapacity( RenderDevice& device )
	{
		Require( ourMaxPushConstant32BitValues == ourExpectedPushConstantValues, "The public push-constant capacity is not 63 values." );

		std::array<uint32_t, ourExpectedPushConstantValues + 1u> values{};
		ICommandBuffer& commands = device.AcquireCommandBuffer();
		commands.CmdPushConstants( values.data(), ourExpectedPushConstantValues * sizeof( uint32_t ) );

		bool pushConstantLimitReached = false;
		try
		{
			commands.CmdPushConstants( values.data(), sizeof( values ) );
		}
		catch( const std::length_error& )
		{
			pushConstantLimitReached = true;
		}
		Require( pushConstantLimitReached, "CmdPushConstants accepted more than 63 values." );

		const SubmitHandle submission = device.Submit( commands );
		device.Wait( submission );
	}

	void TestCommandBufferCapacities( RenderDevice& device )
	{
		Require( ourMaxActiveCommandBuffers == ourExpectedActiveCommandBuffers, "The public active-command-buffer capacity is not 64." );
		Require( ourMaxCommandBufferBatch == ourExpectedCommandBufferBatch, "The public command-buffer batch capacity is not four." );

		std::array<ICommandBuffer*, ourExpectedActiveCommandBuffers> commands{};
		for( ICommandBuffer*& command : commands )
		{
			command = &device.AcquireCommandBuffer();
		}

		bool activeLimitReached = false;
		try
		{
			device.AcquireCommandBuffer();
		}
		catch( const std::length_error& )
		{
			activeLimitReached = true;
		}
		Require( activeLimitReached, "Ldx12 acquired more than 64 active command buffers." );

		bool batchLimitReached = false;
		try
		{
			device.SubmitBatch( commands.data(), ourExpectedCommandBufferBatch + 1u );
		}
		catch( const std::length_error& )
		{
			batchLimitReached = true;
		}
		Require( batchLimitReached, "SubmitBatch accepted more than four command buffers." );

		SubmitHandle submission{};
		for( uint32_t offset = 0; offset < commands.size(); offset += ourExpectedCommandBufferBatch )
		{
			submission = device.SubmitBatch( commands.data() + offset, ourExpectedCommandBufferBatch );
		}
		device.Wait( submission );
	}

	void TestRecordedCapacities()
	{
		ContextDesc context = CreateSmallContext();
		DeviceManagerGuard guard;
		DeviceManager& manager = DeviceManager::Initialize( context );
		guard.active = true;
		RenderDevice& device = *manager.GetRenderDevice();

		TestPipelineAndRenderPassCapacities( device );
		TestTextureTrackingCapacity( device );
		TestPushConstantCapacity( device );
		TestCommandBufferCapacities( device );
		device.WaitIdle();
	}

	bool TryCreateSwapchainWithBufferCount( HWND window, uint32_t bufferCount )
	{
		ContextDesc context = CreateSmallContext();
		context.swapchainBufferCount = bufferCount;
		context.rtvCapacity = ourExpectedMaximumBackbuffers;

		DeviceManagerGuard guard;
		DeviceManager& manager = DeviceManager::Initialize( context );
		guard.active = true;

		SwapchainDesc swapchainDesc{};
		swapchainDesc.window = MakeWin32WindowHandle( window );
		swapchainDesc.width = 64;
		swapchainDesc.height = 64;
		swapchainDesc.vsync = false;

		try
		{
			const SwapchainHandle swapchain = manager.CreateSwapchain( swapchainDesc );
			Require( manager.GetRenderDevice()->GetCurrentSwapchainTexture( swapchain ).Valid(),
				"A supported swapchain did not expose its current backbuffer." );
			manager.DestroySwapchain( swapchain );
			return true;
		}
		catch( const std::runtime_error& )
		{
			return false;
		}
	}

	void TestBackbufferCounts( const TestWindows& windows )
	{
		Require( !TryCreateSwapchainWithBufferCount( windows.Get( 0 ), ourExpectedMinimumBackbuffers - 1u ),
			"A flip-model swapchain accepted only one backbuffer." );
		Require( TryCreateSwapchainWithBufferCount( windows.Get( 0 ), ourExpectedMinimumBackbuffers ),
			"A swapchain with two backbuffers could not be created." );
		Require( TryCreateSwapchainWithBufferCount( windows.Get( 0 ), ourExpectedMaximumBackbuffers ),
			"A swapchain with three backbuffers could not be created." );
		Require( !TryCreateSwapchainWithBufferCount( windows.Get( 0 ), ourExpectedMaximumBackbuffers + 1u ),
			"A swapchain accepted more than three backbuffers." );
	}

	void TestLiveSwapchainCapacity( const TestWindows& windows )
	{
		ContextDesc context = CreateSmallContext();
		context.swapchainBufferCount = ourExpectedMinimumBackbuffers;
		context.rtvCapacity = ourExpectedLiveSwapchains * ourExpectedMinimumBackbuffers;

		DeviceManagerGuard guard;
		DeviceManager& manager = DeviceManager::Initialize( context );
		guard.active = true;

		SwapchainDesc swapchainDesc{};
		swapchainDesc.width = 64;
		swapchainDesc.height = 64;
		swapchainDesc.vsync = false;

		std::array<SwapchainHandle, ourExpectedLiveSwapchains> swapchains{};
		for( uint32_t index = 0; index < swapchains.size(); ++index )
		{
			swapchainDesc.window = MakeWin32WindowHandle( windows.Get( index ) );
			swapchains[ index ] = manager.CreateSwapchain( swapchainDesc );
			Require( swapchains[ index ].Valid(), "A swapchain below the documented live limit was invalid." );
		}

		bool swapchainLimitReached = false;
		try
		{
			swapchainDesc.window = MakeWin32WindowHandle( windows.Get( ourExpectedLiveSwapchains ) );
			manager.CreateSwapchain( swapchainDesc );
		}
		catch( const std::length_error& )
		{
			swapchainLimitReached = true;
		}
		Require( swapchainLimitReached, "Ldx12 accepted more than 16 live swapchains." );
	}
}

int main()
{
	try
	{
		const auto startTime = std::chrono::steady_clock::now();
		TestLiveBufferAndTextureCapacities();
		std::cout << "[PASS] 4,096 live buffers and textures\n";

		TestRecordedCapacities();
		std::cout << "[PASS] render, pipeline and command-buffer capacities\n";

		const TestWindows windows;
		TestBackbufferCounts( windows );
		std::cout << "[PASS] 2-3 backbuffers per swapchain\n";

		TestLiveSwapchainCapacity( windows );
		std::cout << "[PASS] 16 live swapchains\n";

		const double elapsedSeconds = std::chrono::duration<double>( std::chrono::steady_clock::now() - startTime ).count();
		std::cout << "Ldx12 full-capacity tests passed in " << elapsedSeconds << " seconds.\n";
		return 0;
	}
	catch( const std::exception& exception )
	{
		std::cerr << "Ldx12 full-capacity tests failed: " << exception.what() << '\n';
		DeviceManager::ShutdownSingleton();
		return 1;
	}
}
