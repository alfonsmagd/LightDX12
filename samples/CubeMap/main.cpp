#include "Ldx12/HLSLLoader.hpp"
#include "Ldx12/Ldx12.hpp"

#include <DirectXMath.h>
#include <windowsx.h>
#include <wincodec.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

using namespace DirectX;
using namespace ldx12;

namespace
{
	struct Vertex
	{
		XMFLOAT3 position{};
		XMFLOAT3 normal{};
	};

	struct PushConstants
	{
		XMFLOAT4X4 viewProjection{};
		XMFLOAT4X4 skyViewProjection{};
		XMFLOAT4X4 model{};
		XMFLOAT4 cameraPosition{};
		uint32_t cubeMapIndex = 0;
		uint32_t samplerIndex = 0;
		std::array<uint32_t, 2> padding = {};
	};

	static_assert(sizeof( PushConstants ) == 224);

	struct ImageRgba8
	{
		uint32_t width = 0;
		uint32_t height = 0;
		std::vector<uint8_t> pixels;
	};

	struct CubeMapPixels
	{
		uint32_t width = 0;
		uint32_t height = 0;
		uint32_t rowPitch = 0;
		uint32_t slicePitch = 0;
		std::vector<uint8_t> pixels;
	};

	struct DepthTarget
	{
		TextureHandle texture{};
		uint32_t width = 0;
		uint32_t height = 0;
	};

	struct MeshBuffers
	{
		BufferHandle vertexBuffer{};
		BufferHandle indexBuffer{};
		uint32_t indexCount = 0;
	};

	struct OrbitCamera
	{
		float yaw = 0.7f;
		float pitch = 0.44f;
		float distance = 6.5f;
		POINT previousMouse{};
		bool rotating = false;
	};

	struct AppState
	{
		DeviceManager* manager = nullptr;
		RenderPipelineState objectPipeline;
		RenderPipelineState skyboxPipeline;
		TextureHandle cubeMap{};
		MeshBuffers cube;
		MeshBuffers sphere;
		DepthTarget depthTarget;
		OrbitCamera camera;
		bool showSphere = false;
		bool running = true;
		bool minimized = false;
	};

	constexpr std::array<Vertex, 24> ourCubeVertices = {
		Vertex{ { -1.0f, -1.0f, -1.0f }, {  0.0f,  0.0f, -1.0f } },
		Vertex{ { -1.0f,  1.0f, -1.0f }, {  0.0f,  0.0f, -1.0f } },
		Vertex{ {  1.0f,  1.0f, -1.0f }, {  0.0f,  0.0f, -1.0f } },
		Vertex{ {  1.0f, -1.0f, -1.0f }, {  0.0f,  0.0f, -1.0f } },
		Vertex{ {  1.0f, -1.0f,  1.0f }, {  0.0f,  0.0f,  1.0f } },
		Vertex{ {  1.0f,  1.0f,  1.0f }, {  0.0f,  0.0f,  1.0f } },
		Vertex{ { -1.0f,  1.0f,  1.0f }, {  0.0f,  0.0f,  1.0f } },
		Vertex{ { -1.0f, -1.0f,  1.0f }, {  0.0f,  0.0f,  1.0f } },
		Vertex{ { -1.0f, -1.0f,  1.0f }, { -1.0f,  0.0f,  0.0f } },
		Vertex{ { -1.0f,  1.0f,  1.0f }, { -1.0f,  0.0f,  0.0f } },
		Vertex{ { -1.0f,  1.0f, -1.0f }, { -1.0f,  0.0f,  0.0f } },
		Vertex{ { -1.0f, -1.0f, -1.0f }, { -1.0f,  0.0f,  0.0f } },
		Vertex{ {  1.0f, -1.0f, -1.0f }, {  1.0f,  0.0f,  0.0f } },
		Vertex{ {  1.0f,  1.0f, -1.0f }, {  1.0f,  0.0f,  0.0f } },
		Vertex{ {  1.0f,  1.0f,  1.0f }, {  1.0f,  0.0f,  0.0f } },
		Vertex{ {  1.0f, -1.0f,  1.0f }, {  1.0f,  0.0f,  0.0f } },
		Vertex{ { -1.0f,  1.0f, -1.0f }, {  0.0f,  1.0f,  0.0f } },
		Vertex{ { -1.0f,  1.0f,  1.0f }, {  0.0f,  1.0f,  0.0f } },
		Vertex{ {  1.0f,  1.0f,  1.0f }, {  0.0f,  1.0f,  0.0f } },
		Vertex{ {  1.0f,  1.0f, -1.0f }, {  0.0f,  1.0f,  0.0f } },
		Vertex{ { -1.0f, -1.0f,  1.0f }, {  0.0f, -1.0f,  0.0f } },
		Vertex{ { -1.0f, -1.0f, -1.0f }, {  0.0f, -1.0f,  0.0f } },
		Vertex{ {  1.0f, -1.0f, -1.0f }, {  0.0f, -1.0f,  0.0f } },
		Vertex{ {  1.0f, -1.0f,  1.0f }, {  0.0f, -1.0f,  0.0f } }
	};

	constexpr std::array<uint32_t, 36> ourCubeIndices = {
		0, 1, 2, 0, 2, 3,
		4, 5, 6, 4, 6, 7,
		8, 9, 10, 8, 10, 11,
		12, 13, 14, 12, 14, 15,
		16, 17, 18, 16, 18, 19,
		20, 21, 22, 20, 22, 23
	};

	void ThrowIfFailed( HRESULT result, const char* message )
	{
		if( FAILED( result ) )
		{
			throw std::runtime_error( message );
		}
	}

	ImageRgba8 LoadPngRgba8( IWICImagingFactory* factory, const std::filesystem::path& path )
	{
		Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
		ThrowIfFailed( factory->CreateDecoderFromFilename(
			path.c_str(),
			nullptr,
			GENERIC_READ,
			WICDecodeMetadataCacheOnLoad,
			decoder.GetAddressOf() ),
			"Failed to open a cubemap face." );

		Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
		ThrowIfFailed( decoder->GetFrame( 0, frame.GetAddressOf() ),
					   "Failed to decode a cubemap face." );

		ImageRgba8 image{};
		ThrowIfFailed( frame->GetSize( &image.width, &image.height ),
					   "Failed to read cubemap face dimensions." );

		Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
		ThrowIfFailed( factory->CreateFormatConverter( converter.GetAddressOf() ),
					   "Failed to create a WIC format converter." );

		ThrowIfFailed( converter->Initialize(
			frame.Get(),
			GUID_WICPixelFormat32bppRGBA,
			WICBitmapDitherTypeNone,
			nullptr,
			0.0,
			WICBitmapPaletteTypeCustom ),
			"Failed to convert a cubemap face to RGBA8." );

		const uint32_t rowPitch = image.width * 4u;
		const uint32_t imageSize = rowPitch * image.height;
		image.pixels.resize( imageSize );

		ThrowIfFailed( converter->CopyPixels(
			nullptr,
			rowPitch,
			imageSize,
			image.pixels.data() ),
			"Failed to copy cubemap face pixels." );

		return image;
	}

	CubeMapPixels LoadCubeMapPixels( const std::filesystem::path& directory )
	{
		Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
		ThrowIfFailed(
			CoCreateInstance(
			CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS( factory.GetAddressOf() ) ),
			"Failed to create the WIC imaging factory." );

		// D3D12 cubemap subresource order: +X, -X, +Y, -Y, +Z, -Z.
		const std::array<std::filesystem::path, ourCubeMapFaceCount> faceNames = {
			"px.png", "nx.png", "py.png", "ny.png", "pz.png", "nz.png"
		};

		CubeMapPixels cubeMap{};
		for( uint32_t face = 0; face < ourCubeMapFaceCount; ++face )
		{
			const ImageRgba8 image = LoadPngRgba8( factory.Get(), directory / faceNames[ face ] );
			if( face == 0 )
			{
				cubeMap.width = image.width;
				cubeMap.height = image.height;
				cubeMap.rowPitch = image.width * 4u;
				cubeMap.slicePitch = cubeMap.rowPitch * image.height;
				cubeMap.pixels.resize(
					static_cast< size_t >( cubeMap.slicePitch ) * ourCubeMapFaceCount );
			}
			else if( image.width != cubeMap.width || image.height != cubeMap.height )
			{
				throw std::runtime_error( "All cubemap faces must have identical dimensions." );
			}

			std::memcpy( cubeMap.pixels.data() + static_cast< size_t >(face) * cubeMap.slicePitch,
						 image.pixels.data(),
						 cubeMap.slicePitch );
		}
		return cubeMap;
	}

	LRESULT CALLBACK WindowProc( HWND window, UINT message, WPARAM wParam, LPARAM lParam )
	{
		AppState* app = reinterpret_cast< AppState* >(GetWindowLongPtr( window, GWLP_USERDATA ));
		switch( message )
		{
			case WM_LBUTTONDOWN:
			if( app != nullptr )
			{
				app->camera.rotating = true;
				app->camera.previousMouse = { GET_X_LPARAM( lParam ), GET_Y_LPARAM( lParam ) };
				SetCapture( window );
			}
			return 0;

			case WM_LBUTTONUP:
			if( app != nullptr )
			{
				app->camera.rotating = false;
				ReleaseCapture();
			}
			return 0;

			case WM_MOUSEMOVE:
			if( app != nullptr && app->camera.rotating )
			{
				constexpr float sensitivity = 0.005f;
				const POINT mouse = { GET_X_LPARAM( lParam ), GET_Y_LPARAM( lParam ) };

				app->camera.yaw += static_cast< float >(mouse.x - app->camera.previousMouse.x) * sensitivity;
				app->camera.pitch += static_cast< float >(mouse.y - app->camera.previousMouse.y) * sensitivity;

				if( app->camera.pitch < -1.4f ) app->camera.pitch = -1.4f;
				if( app->camera.pitch > 1.4f ) app->camera.pitch = 1.4f;

				app->camera.previousMouse = mouse;
			}
			return 0;

			case WM_CAPTURECHANGED:
			if( app != nullptr )
			{
				app->camera.rotating = false;
			}
			return 0;

			case WM_KEYDOWN:
			if( app != nullptr && wParam == VK_SPACE && (lParam & (1LL << 30)) == 0 )
			{
				app->showSphere = !app->showSphere;
				SetWindowTextW( window, app->showSphere ?
								L"Ldx12 - Cube Map - Sphere" : L"Ldx12 - Cube Map - Cube" );
			}
			return 0;

			case WM_SIZE:
			if( app != nullptr && app->manager != nullptr )
			{
				const uint32_t width = LOWORD( lParam );
				const uint32_t height = HIWORD( lParam );
				app->minimized = width == 0 || height == 0;
				if( !app->minimized )
				{
					app->manager->Resize( width, height );
				}
			}
			return 0;

			case WM_CLOSE:
			if( app != nullptr )
			{
				app->running = false;
			}
			return 0;

			case WM_DESTROY:
			PostQuitMessage( 0 );
			return 0;

			default:
			return DefWindowProc( window, message, wParam, lParam );
		}
	}

	RenderPipelineState CreateObjectPipeline( RenderDevice& device, DXGI_FORMAT colorFormat )
	{
		RenderPipelineDesc desc{};
		desc.vertexShader = HLSLLoader::LoadStage( "shaders/CubeMapObject.hlsl", "vs_6_6", "VSMain" );
		desc.fragmentShader = HLSLLoader::LoadStage( "shaders/CubeMapObject.hlsl", "ps_6_6", "PSMain" );
		desc.color[ 0 ].format = colorFormat;
		desc.depthFormat = DXGI_FORMAT_D32_FLOAT;
		desc.inputElements[ 0 ].semanticName = "POSITION";
		desc.inputElements[ 0 ].format = DXGI_FORMAT_R32G32B32_FLOAT;
		desc.inputElements[ 0 ].alignedByteOffset = 0;
		desc.inputElements[ 1 ].semanticName = "NORMAL";
		desc.inputElements[ 1 ].format = DXGI_FORMAT_R32G32B32_FLOAT;
		desc.inputElements[ 1 ].alignedByteOffset = sizeof( XMFLOAT3 );
		desc.rasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		desc.depthStencilState.DepthEnable = TRUE;
		desc.depthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
		desc.depthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
		desc.depthStencilState.StencilEnable = FALSE;

		return device.CreateRenderPipeline( desc );
	}

	RenderPipelineState CreateSkyboxPipeline( RenderDevice& device, DXGI_FORMAT colorFormat )
	{
		RenderPipelineDesc desc{};
		desc.vertexShader = HLSLLoader::LoadStage( "shaders/CubeMapSkybox.hlsl", "vs_6_6", "VSMain" );
		desc.fragmentShader = HLSLLoader::LoadStage( "shaders/CubeMapSkybox.hlsl", "ps_6_6", "PSMain" );
		desc.color[ 0 ].format = colorFormat;
		desc.depthFormat = DXGI_FORMAT_D32_FLOAT;
		desc.rasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		desc.depthStencilState.DepthEnable = TRUE;
		desc.depthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
		desc.depthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		desc.depthStencilState.StencilEnable = FALSE;

		return device.CreateRenderPipeline( desc );
	}

	void CreateCubeBuffers( AppState& app, RenderDevice& device )
	{
		BufferDesc vertexDesc{};
		vertexDesc.debugName = "Cubemap object vertices";
		vertexDesc.size = sizeof( ourCubeVertices );
		vertexDesc.stride = sizeof( Vertex );
		vertexDesc.type = BufferType::Vertex;
		vertexDesc.initialData = ourCubeVertices.data();
		app.cube.vertexBuffer = device.CreateBuffer( vertexDesc );

		BufferDesc indexDesc{};
		indexDesc.debugName = "Cubemap object indices";
		indexDesc.size = sizeof( ourCubeIndices );
		indexDesc.stride = sizeof( uint32_t );
		indexDesc.type = BufferType::Index;
		indexDesc.initialData = ourCubeIndices.data();
		app.cube.indexBuffer = device.CreateBuffer( indexDesc );
		app.cube.indexCount = static_cast< uint32_t >(ourCubeIndices.size());
	}

	void CreateSphereBuffers( AppState& app, RenderDevice& device )
	{
		constexpr uint32_t rings = 16;
		constexpr uint32_t segments = 32;
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;
		vertices.reserve( (rings + 1u) * (segments + 1u) );
		indices.reserve( rings * segments * 6u );

		for( uint32_t ring = 0; ring <= rings; ++ring )
		{
			const float latitude = XM_PI * static_cast< float >(ring) / static_cast< float >(rings);
			const float ringRadius = std::sin( latitude );
			const float y = std::cos( latitude );
			for( uint32_t segment = 0; segment <= segments; ++segment )
			{
				const float longitude = XM_2PI * static_cast< float >(segment) /
					static_cast< float >(segments);
				const XMFLOAT3 normal = {
					ringRadius * std::cos( longitude ),
					y,
					ringRadius * std::sin( longitude )
				};
				vertices.push_back( { normal, normal } );
			}
		}

		for( uint32_t ring = 0; ring < rings; ++ring )
		{
			for( uint32_t segment = 0; segment < segments; ++segment )
			{
				const uint32_t current = ring * (segments + 1u) + segment;
				const uint32_t nextRing = current + segments + 1u;
				indices.push_back( current );
				indices.push_back( nextRing );
				indices.push_back( current + 1u );
				indices.push_back( current + 1u );
				indices.push_back( nextRing );
				indices.push_back( nextRing + 1u );
			}
		}

		BufferDesc vertexDesc{};
		vertexDesc.debugName = "Cubemap sphere vertices";
		vertexDesc.size = vertices.size() * sizeof( Vertex );
		vertexDesc.stride = sizeof( Vertex );
		vertexDesc.type = BufferType::Vertex;
		vertexDesc.initialData = vertices.data();
		app.sphere.vertexBuffer = device.CreateBuffer( vertexDesc );

		BufferDesc indexDesc{};
		indexDesc.debugName = "Cubemap sphere indices";
		indexDesc.size = indices.size() * sizeof( uint32_t );
		indexDesc.stride = sizeof( uint32_t );
		indexDesc.type = BufferType::Index;
		indexDesc.initialData = indices.data();
		app.sphere.indexBuffer = device.CreateBuffer( indexDesc );
		app.sphere.indexCount = static_cast< uint32_t >(indices.size());
	}

	TextureHandle CreateCubeMap( RenderDevice& device )
	{
		const std::filesystem::path mediaDirectory =
			std::filesystem::path( __FILE__ ).parent_path().parent_path().parent_path() /
			"media" / "sky_129_cubemap_2k";
		const CubeMapPixels pixels = LoadCubeMapPixels( mediaDirectory );

		TextureDesc desc{};
		desc.debugName = "Sky 129 cubemap";
		desc.width = pixels.width;
		desc.height = pixels.height;
		desc.depthOrArraySize = ourCubeMapFaceCount;
		desc.dimension = TextureDimension::TextureCube;
		desc.format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		desc.usage = TextureUsage::Sampled;
		desc.data = pixels.pixels.data();
		desc.rowPitch = pixels.rowPitch;
		desc.slicePitch = pixels.slicePitch;
		return device.CreateTexture( desc );
	}

	void DestroyDepthTarget( RenderDevice& device, DepthTarget& target )
	{
		if( target.texture.Valid() )
		{
			device.Destroy( target.texture );
			target.texture = {};
		}
		target.width = 0;
		target.height = 0;
	}

	void RecreateDepthTarget( AppState& app )
	{
		RenderDevice& device = *app.manager->GetRenderDevice();
		const uint32_t width = app.manager->GetWidth();
		const uint32_t height = app.manager->GetHeight();
		if( app.depthTarget.texture.Valid() &&
			app.depthTarget.width == width && app.depthTarget.height == height )
		{
			return;
		}

		DestroyDepthTarget( device, app.depthTarget );
		TextureDesc desc{};
		desc.debugName = "Cubemap depth";
		desc.width = width;
		desc.height = height;
		desc.format = DXGI_FORMAT_D32_FLOAT;
		desc.usage = TextureUsage::DepthStencil;
		desc.useClearValue = true;
		desc.clearValue.Format = desc.format;
		desc.clearValue.DepthStencil.Depth = 1.0f;
		app.depthTarget.texture = device.CreateTexture( desc );
		app.depthTarget.width = width;
		app.depthTarget.height = height;
	}

	XMVECTOR GetCameraPosition( const OrbitCamera& camera )
	{
		const float horizontalDistance = camera.distance * std::cos( camera.pitch );
		return XMVectorSet(
			horizontalDistance * std::sin( camera.yaw ),
			camera.distance * std::sin( camera.pitch ),
			horizontalDistance * std::cos( camera.yaw ),
			1.0f );
	}

	PushConstants BuildPushConstants( AppState& app, float time )
	{
		RenderDevice& device = *app.manager->GetRenderDevice();

		const float aspect = static_cast< float >(app.manager->GetWidth()) /
			static_cast< float >(app.manager->GetHeight());

		const XMVECTOR cameraPosition = GetCameraPosition( app.camera );
		const XMVECTOR cameraDirection = XMVector3Normalize( XMVectorNegate( cameraPosition ) );
		const XMVECTOR up = XMVectorSet( 0.0f, 1.0f, 0.0f, 0.0f );

		const XMMATRIX view = XMMatrixLookToLH( cameraPosition, cameraDirection, up );
		const XMMATRIX skyView = XMMatrixLookToLH( XMVectorZero(), cameraDirection, up );
		const XMMATRIX projection = XMMatrixPerspectiveFovLH( XMConvertToRadians( 60.0f ), aspect, 0.1f, 100.0f );

		const XMVECTOR rotationAxis = XMVector3Normalize( XMVectorSet( 1.0f, 1.0f, 1.0f, 0.0f ) );
		const XMMATRIX model = XMMatrixRotationAxis( rotationAxis, time * 0.12f );

		PushConstants constants{};
		XMStoreFloat4x4( &constants.viewProjection, XMMatrixTranspose( view * projection ) );
		XMStoreFloat4x4( &constants.skyViewProjection, XMMatrixTranspose( skyView * projection ) );
		XMStoreFloat4x4( &constants.model, XMMatrixTranspose( model ) );
		XMStoreFloat4( &constants.cameraPosition, cameraPosition );
		constants.cubeMapIndex = device.GetBindlessIndex( app.cubeMap );
		constants.samplerIndex = ToSamplerIndex( SamplerSlot::LinearClamp );

		return constants;
	}
}

int WINAPI wWinMain( HINSTANCE instance, HINSTANCE, PWSTR, int showCommand )
{
	try
	{
		const HRESULT comResult = CoInitializeEx( nullptr, COINIT_MULTITHREADED );
		if( FAILED( comResult ) && comResult != RPC_E_CHANGED_MODE )
		{
			throw std::runtime_error( "Failed to initialize COM." );
		}
		WNDCLASSEXW windowClass{};
		windowClass.cbSize = sizeof( WNDCLASSEXW );
		windowClass.lpfnWndProc = WindowProc;
		windowClass.hInstance = instance;
		windowClass.lpszClassName = L"Ldx12CubeMapWindow";
		windowClass.hCursor = LoadCursor( nullptr, IDC_ARROW );

		RegisterClassExW( &windowClass );

		constexpr uint32_t initialWidth = 1280;
		constexpr uint32_t initialHeight = 720;
		HWND window = CreateWindowExW( 0,
									   windowClass.lpszClassName,
									   L"Ldx12 - Cube Map",
									   WS_OVERLAPPEDWINDOW,
									   CW_USEDEFAULT,
									   CW_USEDEFAULT,
									   static_cast< int >(initialWidth),
									   static_cast< int >(initialHeight),
									   nullptr,
									   nullptr,
									   instance,
									   nullptr );
		if( window == nullptr )
		{
			throw std::runtime_error( "Failed to create the Cube Map window." );
		}

		AppState app{};
		SetWindowLongPtr( window, GWLP_USERDATA, reinterpret_cast< LONG_PTR >(&app) );
		ShowWindow( window, showCommand );
		UpdateWindow( window );
		HLSLLoader::SetRootDirectory( std::filesystem::path( __FILE__ ).parent_path() );

		ContextDesc context{};
		context.enableDebugLayer = true;

		SwapchainDesc swapchain{};
		swapchain.window = MakeWin32WindowHandle( window );
		swapchain.width = initialWidth;
		swapchain.height = initialHeight;
		swapchain.vsync = true;

		app.manager = &DeviceManager::Initialize( context, swapchain );

		RenderDevice& device = *app.manager->GetRenderDevice();
		app.objectPipeline = CreateObjectPipeline( device, context.swapchainFormat );
		app.skyboxPipeline = CreateSkyboxPipeline( device, context.swapchainFormat );
		app.cubeMap = CreateCubeMap( device );

		CreateCubeBuffers( app, device );
		CreateSphereBuffers( app, device );
		RecreateDepthTarget( app );

		const std::chrono::steady_clock::time_point animationStart = std::chrono::steady_clock::now();
		MSG message{};

		while( app.running )
		{
			while( PeekMessage( &message, nullptr, 0, 0, PM_REMOVE ) )
			{
				if( message.message == WM_QUIT )
				{
					app.running = false;
					break;
				}
				TranslateMessage( &message );
				DispatchMessage( &message );
			}

			if( !app.running || app.minimized )
			{
				continue;
			}

			RecreateDepthTarget( app );

			const float time = std::chrono::duration<float>( std::chrono::steady_clock::now() - animationStart ).count();

			const PushConstants constants = BuildPushConstants( app, time );
			const TextureHandle backbuffer = device.GetCurrentSwapchainTexture();

			RenderPass renderPass{};
			renderPass.color[ 0 ].loadOp = LoadOp::Clear;
			renderPass.color[ 0 ].clearColor = { 0.02f, 0.025f, 0.04f, 1.0f };
			renderPass.depthStencil.depthLoadOp = LoadOp::Clear;
			renderPass.depthStencil.clearDepth = 1.0f;

			Framebuffer framebuffer{};
			framebuffer.color[ 0 ].texture = backbuffer;
			framebuffer.depthStencil.texture = app.depthTarget.texture;

			ICommandBuffer& commands = device.AcquireCommandBuffer();
			{
				commands.CmdBeginRendering( renderPass, framebuffer );
				commands.CmdPushConstants( &constants, sizeof( constants ) );

				const MeshBuffers& mesh = app.showSphere ? app.sphere : app.cube;
				commands.CmdBindRenderPipeline( app.objectPipeline );
				commands.CmdBindVertexBuffer( mesh.vertexBuffer );
				commands.CmdBindIndexBuffer( mesh.indexBuffer );
				commands.CmdDrawIndexed( mesh.indexCount );

				commands.CmdBindRenderPipeline( app.skyboxPipeline );
				commands.CmdDraw( 36 );
				commands.CmdEndRendering();
			}

			device.Submit( commands, backbuffer );
		}

		SetWindowLongPtr( window, GWLP_USERDATA, 0 );
		device.WaitIdle();

		DestroyDepthTarget( device, app.depthTarget );

		device.Destroy( app.sphere.indexBuffer );
		device.Destroy( app.sphere.vertexBuffer );
		device.Destroy( app.cube.indexBuffer );
		device.Destroy( app.cube.vertexBuffer );
		device.Destroy( app.cubeMap );

		app.skyboxPipeline = {};
		app.objectPipeline = {};

		DeviceManager::ShutdownSingleton();
		app.manager = nullptr;

		if( IsWindow( window ) != FALSE )
		{
			DestroyWindow( window );
		}
		UnregisterClassW( windowClass.lpszClassName, instance );
		return 0;
	}
	catch( const std::exception& error )
	{
		DeviceManager::ShutdownSingleton();
		MessageBoxA( nullptr, error.what(), "Ldx12 Cube Map failed", MB_ICONERROR | MB_OK );
		return 1;
	}
}
