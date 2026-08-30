#include "Ldx12Internal.hpp"

#include "Ldx12BaseMips.hpp"
#include "Ldx12CommandBuffer.hpp"
#include "Ldx12ImmediateCommands.hpp"
#include "Ldx12StagingDevice.hpp"
#include "Ldx12Swapchain.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <filesystem>
#include <mutex>

namespace ldx12
{
	namespace
	{
		std::mutex gDeviceManagerSingletonMutex;
		std::unique_ptr<DeviceManager> gDeviceManagerSingleton;
		uint32_t gDeviceManagerSingletonReferenceCount = 0;
		constexpr D3D_SHADER_MODEL ourRequiredShaderModel = D3D_SHADER_MODEL_6_6;
		constexpr D3D12_RESOURCE_BINDING_TIER ourRequiredResourceBindingTier = D3D12_RESOURCE_BINDING_TIER_3;

		std::string ToUtf8( const wchar_t* text )
		{
			const int wideLength = static_cast<int>( std::wcslen( text ) );
			const int utf8Length = WideCharToMultiByte( CP_UTF8, 0, text, wideLength, nullptr, 0, nullptr, nullptr );
			if( utf8Length <= 0 )
			{
				return "Unknown adapter";
			}

			std::string utf8Text( static_cast<size_t>( utf8Length ), '\0' );
			WideCharToMultiByte( CP_UTF8, 0, text, wideLength, utf8Text.data(), utf8Length, nullptr, nullptr );
			return utf8Text;
		}

		std::string HResultText( HRESULT result )
		{
			char text[ 11 ] = {};
			std::snprintf( text, sizeof( text ), "0x%08lX", static_cast<unsigned long>( result ) );
			return text;
		}

		std::string FeatureLevelText( D3D_FEATURE_LEVEL featureLevel )
		{
			switch( featureLevel )
			{
			case D3D_FEATURE_LEVEL_12_2:
				return "12.2";
			case D3D_FEATURE_LEVEL_12_1:
				return "12.1";
			case D3D_FEATURE_LEVEL_12_0:
				return "12.0";
			case D3D_FEATURE_LEVEL_11_1:
				return "11.1";
			case D3D_FEATURE_LEVEL_11_0:
				return "11.0";
			default:
				return "unknown";
			}
		}

		std::string ShaderModelText( D3D_SHADER_MODEL shaderModel )
		{
			const uint32_t value = static_cast<uint32_t>( shaderModel );
			return std::to_string( value >> 4u ) + "." + std::to_string( value & 0x0fu );
		}

		bool ContextDescsAreCompatible( const ContextDesc& existingDesc, const ContextDesc& requestedDesc ) noexcept
		{
			return existingDesc.enableDebugLayer == requestedDesc.enableDebugLayer &&
				   existingDesc.preferHighPerformanceAdapter == requestedDesc.preferHighPerformanceAdapter &&
				   existingDesc.allowTearing == requestedDesc.allowTearing &&
				   existingDesc.pixSettings.enableGpuCapture == requestedDesc.pixSettings.enableGpuCapture &&
				   existingDesc.pixSettings.showGpuCaptureHud == requestedDesc.pixSettings.showGpuCaptureHud &&
				   existingDesc.bindlessCapacity == requestedDesc.bindlessCapacity && existingDesc.rtvCapacity == requestedDesc.rtvCapacity &&
				   existingDesc.dsvCapacity == requestedDesc.dsvCapacity && existingDesc.swapchainBufferCount == requestedDesc.swapchainBufferCount &&
				   existingDesc.swapchainFormat == requestedDesc.swapchainFormat && existingDesc.minimumFeatureLevel == requestedDesc.minimumFeatureLevel;
		}

		void ReleaseDeviceManagerSingleton() noexcept
		{
			std::unique_ptr<DeviceManager> singletonToDestroy;
			{
				std::lock_guard lock( gDeviceManagerSingletonMutex );
				if( gDeviceManagerSingletonReferenceCount == 0 )
				{
					return;
				}

				--gDeviceManagerSingletonReferenceCount;
				if( gDeviceManagerSingletonReferenceCount == 0 )
				{
					singletonToDestroy = std::move( gDeviceManagerSingleton );
				}
			}

			singletonToDestroy.reset();
		}

		std::array<uint32_t, 4> ParseVersionComponents( const std::wstring& versionText )
		{
			std::array<uint32_t, 4> components = {};
			uint32_t componentIndex = 0;
			size_t start = 0;
			while( start < versionText.size() && componentIndex < components.size() )
			{
				const size_t end = versionText.find( L'.', start );
				const std::wstring token = versionText.substr( start, end == std::wstring::npos ? std::wstring::npos : end - start );
				components[ componentIndex++ ] = token.empty() ? 0u : static_cast<uint32_t>( std::wcstoul( token.c_str(), nullptr, 10 ) );

				if( end == std::wstring::npos )
				{
					break;
				}

				start = end + 1;
			}

			return components;
		}

		bool IsVersionGreater( const std::array<uint32_t, 4>& left, const std::array<uint32_t, 4>& right )
		{
			for( size_t index = 0; index < left.size(); ++index )
			{
				const uint32_t leftValue = left[ index ];
				const uint32_t rightValue = right[ index ];
				if( leftValue != rightValue )
				{
					return leftValue > rightValue;
				}
			}

			return false;
		}

		bool TryLoadPixGpuCapturerInternal()
		{
			static bool ourAttemptedLoad = false;
			if( ourAttemptedLoad )
			{
				return ::GetModuleHandleW( L"WinPixGpuCapturer.dll" ) != nullptr;
			}

			ourAttemptedLoad = true;
			if( ::GetModuleHandleW( L"WinPixGpuCapturer.dll" ) != nullptr )
			{
				return true;
			}

			// First allow a local copy next to the executable or on PATH.
			if( ::LoadLibraryW( L"WinPixGpuCapturer.dll" ) != nullptr )
			{
				return true;
			}

			wchar_t* programFilesValue = nullptr;
			size_t programFilesLength = 0;
			if( _wdupenv_s( &programFilesValue, &programFilesLength, L"ProgramFiles" ) != 0 || programFilesValue == nullptr || programFilesValue[ 0 ] == L'\0' )
			{
				free( programFilesValue );
				return false;
			}

			const std::filesystem::path pixInstallRoot = std::filesystem::path( programFilesValue ) / "Microsoft PIX";
			free( programFilesValue );
			if( !std::filesystem::exists( pixInstallRoot ) )
			{
				return false;
			}

			std::filesystem::path latestCapturerPath;
			std::array<uint32_t, 4> latestVersion = {};

			for( const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator( pixInstallRoot ) )
			{
				if( !entry.is_directory() )
				{
					continue;
				}

				const std::filesystem::path candidateDll = entry.path() / "WinPixGpuCapturer.dll";
				if( !std::filesystem::exists( candidateDll ) )
				{
					continue;
				}

				const std::array<uint32_t, 4> candidateVersion = ParseVersionComponents( entry.path().filename().wstring() );
				if( latestCapturerPath.empty() || IsVersionGreater( candidateVersion, latestVersion ) )
				{
					latestCapturerPath = candidateDll;
					latestVersion = candidateVersion;
				}
			}

			if( !latestCapturerPath.empty() )
			{
				if( ::LoadLibraryW( latestCapturerPath.c_str() ) != nullptr )
				{
					return true;
				}
			}

			return false;
		}

		bool TrySetPixGpuCaptureHudVisibility( bool visible ) noexcept
		{
			HMODULE capturerModule = ::GetModuleHandleW( L"WinPixGpuCapturer.dll" );
			if( capturerModule == nullptr )
			{
				return false;
			}

			using SetHudOptionsFn = HRESULT( WINAPI* )( uint32_t );
			SetHudOptionsFn setHudOptions = reinterpret_cast<SetHudOptionsFn>( ::GetProcAddress( capturerModule, "SetHUDOptions" ) );
			if( setHudOptions == nullptr )
			{
				return false;
			}

			constexpr uint32_t kShowOnAllWindows = 0x1;
			constexpr uint32_t kShowOnNoWindows = 0x4;
			return SUCCEEDED( setHudOptions( visible ? kShowOnAllWindows : kShowOnNoWindows ) );
		}
	}

	SwapchainResource::SwapchainResource() = default;
	SwapchainResource::~SwapchainResource() = default;
	SwapchainResource::SwapchainResource( SwapchainResource&& other ) noexcept = default;
	SwapchainResource& SwapchainResource::operator=( SwapchainResource&& other ) noexcept = default;

	DeviceManager::QueueContext::QueueContext() = default;
	DeviceManager::QueueContext::~QueueContext() = default;

	bool TryLoadPixGpuCapturer() noexcept
	{
		return TryLoadPixGpuCapturerInternal();
	}

	bool IsPixGpuCapturerLoaded() noexcept
	{
		return ::GetModuleHandleW( L"WinPixGpuCapturer.dll" ) != nullptr;
	}

	void DeviceManager::Initialize()
	{
		if( desc_.pixSettings.enableGpuCapture )
		{
			// PIX GPU capture attach only works if the capturer DLL is loaded before any D3D12 device creation.
			if( TryLoadPixGpuCapturer() )
			{
				TrySetPixGpuCaptureHudVisibility( desc_.pixSettings.showGpuCaptureHud );
			}
		}

		InitializeFactory();
		InitializeDevice();
		std::string capabilityFailure;
		if( !CheckCapabilities( capabilityFailure ) )
		{
			throw std::runtime_error( capabilityFailure );
		}
		InitializeCommandQueues();
		InitializeDescriptorHeaps();
		InitializeRootSignature();
		InitializeCommandSignature();
		baseMips_ = std::make_unique<BaseMips>( *this );
		stagingDevice_ = std::make_unique<StagingDevice>( *this );
	}

	void DeviceManager::InitializeFactory()
	{
		UINT flags = 0;
#if defined( _DEBUG )
		if( desc_.enableDebugLayer )
		{
			ComPtr<ID3D12Debug> debugController;
			if( SUCCEEDED( D3D12GetDebugInterface( IID_PPV_ARGS( debugController.GetAddressOf() ) ) ) )
			{
				debugController->EnableDebugLayer();
				flags |= DXGI_CREATE_FACTORY_DEBUG;
			}
		}
#endif
		C_RESULT( CreateDXGIFactory2( flags, IID_PPV_ARGS( factory_.GetAddressOf() ) ), "Failed to create DXGI factory." );
	}

	void DeviceManager::InitializeDevice()
	{
		auto tryAdapter = [ this ]( IDXGIAdapter1* candidate ) -> bool
		{ return SUCCEEDED( D3D12CreateDevice( candidate, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS( device_.GetAddressOf() ) ) ); };

		if( desc_.preferHighPerformanceAdapter )
		{
			for( UINT adapterIndex = 0;; ++adapterIndex )
			{
				ComPtr<IDXGIAdapter1> candidate;
				if( factory_->EnumAdapterByGpuPreference( adapterIndex, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS( candidate.GetAddressOf() ) ) ==
					DXGI_ERROR_NOT_FOUND )
				{
					break;
				}

				DXGI_ADAPTER_DESC1 adapterDesc{};
				candidate->GetDesc1( &adapterDesc );
				if( ( adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE ) != 0 )
				{
					continue;
				}

				if( tryAdapter( candidate.Get() ) )
				{
					adapter_ = candidate;
					break;
				}
			}
		}

		if( device_ == nullptr )
		{
			for( UINT adapterIndex = 0;; ++adapterIndex )
			{
				ComPtr<IDXGIAdapter1> candidate;
				if( factory_->EnumAdapters1( adapterIndex, candidate.GetAddressOf() ) == DXGI_ERROR_NOT_FOUND )
				{
					break;
				}

				DXGI_ADAPTER_DESC1 adapterDesc{};
				candidate->GetDesc1( &adapterDesc );
				if( ( adapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE ) != 0 )
				{
					continue;
				}

				if( tryAdapter( candidate.Get() ) )
				{
					adapter_ = candidate;
					break;
				}
			}
		}

		if( device_ == nullptr )
		{
			ComPtr<IDXGIAdapter1> warpAdapter;
			C_RESULT( factory_->EnumWarpAdapter( IID_PPV_ARGS( warpAdapter.GetAddressOf() ) ), "Failed to enumerate WARP adapter." );
			C_RESULT( D3D12CreateDevice( warpAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS( device_.GetAddressOf() ) ),
				"Failed to create D3D12 device." );
			adapter_ = warpAdapter;
		}
	}

	bool DeviceManager::CheckCapabilities( std::string& failureReason )
	{
		DXGI_ADAPTER_DESC1 adapterDesc{};
		const HRESULT adapterResult = adapter_->GetDesc1( &adapterDesc );
		if( FAILED( adapterResult ) )
		{
			failureReason = "Ldx12 failed to read the selected adapter properties (HRESULT " + HResultText( adapterResult ) + ").";
			return false;
		}

		deviceProperties_.adapterName = ToUtf8( adapterDesc.Description );
		deviceProperties_.dedicatedVideoMemoryBytes = static_cast<uint64_t>( adapterDesc.DedicatedVideoMemory );
		deviceProperties_.dedicatedSystemMemoryBytes = static_cast<uint64_t>( adapterDesc.DedicatedSystemMemory );
		deviceProperties_.sharedSystemMemoryBytes = static_cast<uint64_t>( adapterDesc.SharedSystemMemory );
		const std::string adapterPrefix = "Ldx12 cannot initialize adapter \"" + deviceProperties_.adapterName + "\": ";

		const D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_12_2,
			D3D_FEATURE_LEVEL_12_1,
			D3D_FEATURE_LEVEL_12_0,
			D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_11_0 };
		D3D12_FEATURE_DATA_FEATURE_LEVELS featureLevelData{};
		featureLevelData.NumFeatureLevels = static_cast<UINT>( std::size( featureLevels ) );
		featureLevelData.pFeatureLevelsRequested = featureLevels;
		const HRESULT featureLevelResult = device_->CheckFeatureSupport( D3D12_FEATURE_FEATURE_LEVELS, &featureLevelData, sizeof( featureLevelData ) );
		if( FAILED( featureLevelResult ) )
		{
			failureReason = adapterPrefix + "the feature-level query failed (HRESULT " + HResultText( featureLevelResult ) + ").";
			return false;
		}

		deviceProperties_.featureLevel = featureLevelData.MaxSupportedFeatureLevel;
		if( deviceProperties_.featureLevel < desc_.minimumFeatureLevel )
		{
			failureReason = adapterPrefix + "feature level " + FeatureLevelText( desc_.minimumFeatureLevel ) + " is required, but the adapter supports " +
							FeatureLevelText( deviceProperties_.featureLevel ) + ".";
			return false;
		}

		const D3D_SHADER_MODEL shaderModels[] = { D3D_SHADER_MODEL_6_6,
			D3D_SHADER_MODEL_6_5,
			D3D_SHADER_MODEL_6_4,
			D3D_SHADER_MODEL_6_3,
			D3D_SHADER_MODEL_6_2,
			D3D_SHADER_MODEL_6_1,
			D3D_SHADER_MODEL_6_0,
			D3D_SHADER_MODEL_5_1 };
		HRESULT shaderModelResult = E_INVALIDARG;
		for( D3D_SHADER_MODEL shaderModel : shaderModels )
		{
			D3D12_FEATURE_DATA_SHADER_MODEL shaderModelData{};
			shaderModelData.HighestShaderModel = shaderModel;
			shaderModelResult = device_->CheckFeatureSupport( D3D12_FEATURE_SHADER_MODEL, &shaderModelData, sizeof( shaderModelData ) );
			if( SUCCEEDED( shaderModelResult ) )
			{
				deviceProperties_.shaderModel = shaderModelData.HighestShaderModel;
				break;
			}
		}

		if( FAILED( shaderModelResult ) )
		{
			failureReason = adapterPrefix + "the Shader Model query failed (HRESULT " + HResultText( shaderModelResult ) + ").";
			return false;
		}
		if( deviceProperties_.shaderModel < ourRequiredShaderModel )
		{
			failureReason = adapterPrefix + "Shader Model " + ShaderModelText( ourRequiredShaderModel ) + " is required, but the adapter supports " +
							ShaderModelText( deviceProperties_.shaderModel ) + ".";
			return false;
		}

		D3D12_FEATURE_DATA_D3D12_OPTIONS options{};
		const HRESULT optionsResult = device_->CheckFeatureSupport( D3D12_FEATURE_D3D12_OPTIONS, &options, sizeof( options ) );
		if( FAILED( optionsResult ) )
		{
			failureReason = adapterPrefix + "the Resource Binding Tier query failed (HRESULT " + HResultText( optionsResult ) + ").";
			return false;
		}

		deviceProperties_.resourceBindingTier = options.ResourceBindingTier;
		if( deviceProperties_.resourceBindingTier < ourRequiredResourceBindingTier )
		{
			failureReason = adapterPrefix + "Resource Binding Tier " + std::to_string( static_cast<uint32_t>( ourRequiredResourceBindingTier ) ) +
							" is required, but the adapter supports Tier " + std::to_string( static_cast<uint32_t>( deviceProperties_.resourceBindingTier ) ) +
							".";
			return false;
		}

		failureReason.clear();
		return true;
	}

	void DeviceManager::InitializeCommandQueues()
	{
		InitializeQueueContext( graphicsQueue_, D3D12_COMMAND_LIST_TYPE_DIRECT );
	}

	void DeviceManager::InitializeQueueContext( QueueContext& context, D3D12_COMMAND_LIST_TYPE type )
	{
		D3D12_COMMAND_QUEUE_DESC queueDesc{};
		queueDesc.Type = type;
		C_RESULT( device_->CreateCommandQueue( &queueDesc, IID_PPV_ARGS( context.commandQueue_.GetAddressOf() ) ), "Failed to create command queue." );
		C_RESULT( device_->CreateFence( 0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS( context.queueIdleFence_.GetAddressOf() ) ),
			"Failed to create queue idle fence." );
		context.queueIdleEvent_ = CreateEvent( nullptr, FALSE, FALSE, nullptr );
		if( context.queueIdleEvent_ == nullptr )
		{
			throw std::runtime_error( "Failed to create queue idle event." );
		}

		context.immediateCommands_ = std::make_unique<ImmediateCommands>( device_.Get(), context.commandQueue_.Get(), ourMaxImmediateCommandBuffers );
	}

	DeviceManager::QueueContext& DeviceManager::GetGraphicsQueueContext() noexcept
	{
		return graphicsQueue_;
	}

	const DeviceManager::QueueContext& DeviceManager::GetGraphicsQueueContext() const noexcept
	{
		return graphicsQueue_;
	}

	void DeviceManager::InitializeDescriptorHeaps()
	{
		if( desc_.bindlessCapacity <= LDX12_BINDLESS_FIXED_SLOT_LAST )
		{
			throw std::runtime_error( "Bindless descriptor capacity must include Ldx12 fixed binding slots." );
		}
		if( desc_.bindlessCapacity > ourMaxBindlessDescriptors || desc_.rtvCapacity > ourMaxRtvDescriptors || desc_.dsvCapacity > ourMaxDsvDescriptors )
		{
			throw std::length_error( "Descriptor capacities exceed the fixed Ldx12 array limits." );
		}

		D3D12_DESCRIPTOR_HEAP_DESC bindlessDesc{};
		bindlessDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		bindlessDesc.NumDescriptors = desc_.bindlessCapacity;
		bindlessDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		C_RESULT( device_->CreateDescriptorHeap( &bindlessDesc, IID_PPV_ARGS( bindlessHeap_.GetAddressOf() ) ), "Failed to create bindless heap." );

		D3D12_DESCRIPTOR_HEAP_DESC samplerHeapDesc{};
		samplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
		samplerHeapDesc.NumDescriptors = ourMaxSamplers;
		samplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		C_RESULT( device_->CreateDescriptorHeap( &samplerHeapDesc, IID_PPV_ARGS( samplerHeap_.GetAddressOf() ) ), "Failed to create sampler heap." );

		D3D12_DESCRIPTOR_HEAP_DESC rtvDesc{};
		rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		rtvDesc.NumDescriptors = desc_.rtvCapacity;
		C_RESULT( device_->CreateDescriptorHeap( &rtvDesc, IID_PPV_ARGS( rtvHeap_.GetAddressOf() ) ), "Failed to create RTV heap." );

		D3D12_DESCRIPTOR_HEAP_DESC dsvDesc{};
		dsvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		dsvDesc.NumDescriptors = desc_.dsvCapacity;
		C_RESULT( device_->CreateDescriptorHeap( &dsvDesc, IID_PPV_ARGS( dsvHeap_.GetAddressOf() ) ), "Failed to create DSV heap." );

		bindlessDescriptorSize_ = device_->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
		samplerDescriptorSize_ = device_->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER );
		rtvDescriptorSize_ = device_->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_RTV );
		dsvDescriptorSize_ = device_->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_DSV );

		std::array<SamplerDesc, ourBuiltInSamplerCount> samplerDescs{};
		samplerDescs[ ToSamplerIndex( SamplerSlot::LinearWrap ) ].addressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDescs[ ToSamplerIndex( SamplerSlot::LinearWrap ) ].addressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDescs[ ToSamplerIndex( SamplerSlot::LinearWrap ) ].addressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		samplerDescs[ ToSamplerIndex( SamplerSlot::PointClamp ) ].filter = D3D12_FILTER_MIN_MAG_MIP_POINT;

		SamplerDesc& shadowSampler = samplerDescs[ ToSamplerIndex( SamplerSlot::ShadowComparison ) ];
		shadowSampler.filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
		shadowSampler.addressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		shadowSampler.addressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		shadowSampler.addressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		shadowSampler.comparisonFunction = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		shadowSampler.borderColor = { 1.0f, 1.0f, 1.0f, 1.0f };

		for( uint32_t index = 0; index < ourBuiltInSamplerCount; ++index )
		{
			WriteSamplerDescriptor( index, samplerDescs[ index ] );
		}

		freeBindlessRangeCount_ = 0;
		const uint32_t dynamicDescriptorCount = desc_.bindlessCapacity - LDX12_BINDLESS_DYNAMIC_SLOT_FIRST;
		if( dynamicDescriptorCount > 0 )
		{
			freeBindlessRanges_[ freeBindlessRangeCount_++ ] = DescriptorRange{ .start_ = LDX12_BINDLESS_DYNAMIC_SLOT_FIRST, .count_ = dynamicDescriptorCount };
		}
		fixedBindlessDescriptorUsed_.fill( 0u );

		freeRtvDescriptorCount_ = desc_.rtvCapacity;
		for( uint32_t index = desc_.rtvCapacity; index > 0; --index )
		{
			freeRtvDescriptors_[ desc_.rtvCapacity - index ] = index - 1u;
		}

		freeDsvDescriptorCount_ = desc_.dsvCapacity;
		for( uint32_t index = desc_.dsvCapacity; index > 0; --index )
		{
			freeDsvDescriptors_[ desc_.dsvCapacity - index ] = index - 1u;
		}
	}

	void DeviceManager::InitializeRootSignature()
	{
		D3D12_ROOT_PARAMETER1 parameters[ 2 ] = {};
		parameters[ 0 ].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		parameters[ 0 ].Constants.Num32BitValues = ourMaxPushConstant32BitValues;
		parameters[ 0 ].Constants.ShaderRegister = 0;
		parameters[ 0 ].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		parameters[ 1 ].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
		parameters[ 1 ].Constants.Num32BitValues = 1;
		parameters[ 1 ].Constants.ShaderRegister = 1;
		parameters[ 1 ].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

		D3D12_VERSIONED_ROOT_SIGNATURE_DESC rootDesc{};
		rootDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
		rootDesc.Desc_1_1.NumParameters = 2;
		rootDesc.Desc_1_1.pParameters = parameters;
		rootDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | D3D12_ROOT_SIGNATURE_FLAG_CBV_SRV_UAV_HEAP_DIRECTLY_INDEXED |
								  D3D12_ROOT_SIGNATURE_FLAG_SAMPLER_HEAP_DIRECTLY_INDEXED;

		ComPtr<ID3DBlob> serialized;
		ComPtr<ID3DBlob> errors;
		C_RESULT( D3D12SerializeVersionedRootSignature( &rootDesc, serialized.GetAddressOf(), errors.GetAddressOf() ), "Failed to serialize root signature." );
		C_RESULT( device_->CreateRootSignature( 0, serialized->GetBufferPointer(), serialized->GetBufferSize(), IID_PPV_ARGS( rootSignature_.GetAddressOf() ) ),
			"Failed to create root signature." );
	}

	void DeviceManager::InitializeCommandSignature()
	{
		D3D12_INDIRECT_ARGUMENT_DESC arguments[ 2 ] = {};
		arguments[ 0 ].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT;
		arguments[ 0 ].Constant.RootParameterIndex = 1;
		arguments[ 0 ].Constant.Num32BitValuesToSet = 1;
		arguments[ 1 ].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

		D3D12_COMMAND_SIGNATURE_DESC signatureDesc{};
		signatureDesc.ByteStride = sizeof( uint32_t ) + sizeof( D3D12_DRAW_INDEXED_ARGUMENTS );
		signatureDesc.NumArgumentDescs = 2;
		signatureDesc.pArgumentDescs = arguments;

		C_RESULT( device_->CreateCommandSignature( &signatureDesc, rootSignature_.Get(), IID_PPV_ARGS( commandSignature_.GetAddressOf() ) ),
			"Failed to create command signature." );
	}

	uint32_t DeviceManager::AllocateBindlessDescriptor()
	{
		return AllocateBindlessDescriptorRange( 1u );
	}

	uint32_t DeviceManager::AllocateBindlessDescriptorRange( uint32_t count )
	{
		if( count == 0 )
		{
			throw std::runtime_error( "Bindless descriptor allocation count must be greater than zero." );
		}

		for( uint32_t rangeIndex = 0; rangeIndex < freeBindlessRangeCount_; ++rangeIndex )
		{
			DescriptorRange& range = freeBindlessRanges_[ rangeIndex ];
			if( range.count_ < count )
			{
				continue;
			}

			const uint32_t start = range.start_;
			range.start_ += count;
			range.count_ -= count;
			if( range.count_ == 0 )
			{
				EraseFreeBindlessRange( rangeIndex );
			}

			return start;
		}

		throw std::runtime_error( "Bindless descriptor heap is exhausted." );
	}

	uint32_t DeviceManager::AllocateFixedBindlessDescriptor( uint32_t index )
	{
		if( index < LDX12_BINDLESS_FIXED_SLOT_FIRST || index > LDX12_BINDLESS_FIXED_SLOT_LAST || index >= fixedBindlessDescriptorUsed_.size() )
		{
			throw std::runtime_error( "Invalid fixed bindless descriptor slot." );
		}

		if( fixedBindlessDescriptorUsed_[ index ] != 0u )
		{
			throw std::runtime_error( "Fixed bindless descriptor slot is already in use." );
		}

		fixedBindlessDescriptorUsed_[ index ] = 1u;
		return index;
	}

	uint32_t DeviceManager::AllocateRtvDescriptor()
	{
		if( freeRtvDescriptorCount_ == 0 )
		{
			throw std::runtime_error( "RTV descriptor heap is exhausted." );
		}

		return freeRtvDescriptors_[ --freeRtvDescriptorCount_ ];
	}

	uint32_t DeviceManager::AllocateDsvDescriptor()
	{
		if( freeDsvDescriptorCount_ == 0 )
		{
			throw std::runtime_error( "DSV descriptor heap is exhausted." );
		}

		return freeDsvDescriptors_[ --freeDsvDescriptorCount_ ];
	}

	void DeviceManager::FreeBindlessDescriptor( uint32_t index )
	{
		FreeBindlessDescriptorRange( index, 1u );
	}

	void DeviceManager::FreeBindlessDescriptorRange( uint32_t index, uint32_t count )
	{
		if( index == UINT32_MAX || count == 0 )
		{
			return;
		}

		if( count == 1u && index >= LDX12_BINDLESS_FIXED_SLOT_FIRST && index <= LDX12_BINDLESS_FIXED_SLOT_LAST && index < fixedBindlessDescriptorUsed_.size() )
		{
			fixedBindlessDescriptorUsed_[ index ] = 0u;
			return;
		}

		if( freeBindlessRangeCount_ == freeBindlessRanges_.size() )
		{
			throw std::length_error( "Bindless free-range array is exhausted." );
		}

		DescriptorRange mergedRange{ .start_ = index, .count_ = count };
		uint32_t insertIndex = 0;
		while( insertIndex < freeBindlessRangeCount_ && freeBindlessRanges_[ insertIndex ].start_ < mergedRange.start_ )
		{
			++insertIndex;
		}
		for( uint32_t moveIndex = freeBindlessRangeCount_; moveIndex > insertIndex; --moveIndex )
		{
			freeBindlessRanges_[ moveIndex ] = freeBindlessRanges_[ moveIndex - 1u ];
		}
		freeBindlessRanges_[ insertIndex ] = mergedRange;
		++freeBindlessRangeCount_;

		if( insertIndex > 0 )
		{
			DescriptorRange& previous = freeBindlessRanges_[ insertIndex - 1u ];
			DescriptorRange& inserted = freeBindlessRanges_[ insertIndex ];
			if( previous.start_ + previous.count_ == inserted.start_ )
			{
				previous.count_ += inserted.count_;
				EraseFreeBindlessRange( insertIndex );
				--insertIndex;
			}
		}

		if( insertIndex + 1u < freeBindlessRangeCount_ )
		{
			DescriptorRange& inserted = freeBindlessRanges_[ insertIndex ];
			const DescriptorRange& next = freeBindlessRanges_[ insertIndex + 1u ];
			if( inserted.start_ + inserted.count_ == next.start_ )
			{
				inserted.count_ += next.count_;
				EraseFreeBindlessRange( insertIndex + 1u );
			}
		}
	}

	void DeviceManager::WriteSamplerDescriptor( uint32_t index, const SamplerDesc& desc )
	{
		assert( index < ourMaxSamplers );
		D3D12_SAMPLER_DESC nativeDesc{};
		nativeDesc.Filter = desc.filter;
		nativeDesc.AddressU = desc.addressU;
		nativeDesc.AddressV = desc.addressV;
		nativeDesc.AddressW = desc.addressW;
		nativeDesc.MipLODBias = desc.mipLodBias;
		nativeDesc.MaxAnisotropy = desc.maxAnisotropy;
		nativeDesc.ComparisonFunc = desc.comparisonFunction;
		std::copy( desc.borderColor.begin(), desc.borderColor.end(), nativeDesc.BorderColor );
		nativeDesc.MinLOD = desc.minLod;
		nativeDesc.MaxLOD = desc.maxLod;

		D3D12_CPU_DESCRIPTOR_HANDLE handle = samplerHeap_->GetCPUDescriptorHandleForHeapStart();
		handle.ptr += static_cast<SIZE_T>( index ) * samplerDescriptorSize_;
		device_->CreateSampler( &nativeDesc, handle );
	}

	void DeviceManager::EraseFreeBindlessRange( uint32_t rangeIndex ) noexcept
	{
		assert( rangeIndex < freeBindlessRangeCount_ );
		for( uint32_t moveIndex = rangeIndex + 1u; moveIndex < freeBindlessRangeCount_; ++moveIndex )
		{
			freeBindlessRanges_[ moveIndex - 1u ] = freeBindlessRanges_[ moveIndex ];
		}
		--freeBindlessRangeCount_;
		freeBindlessRanges_[ freeBindlessRangeCount_ ] = {};
	}

	void DeviceManager::FreeRtvDescriptor( uint32_t index )
	{
		if( index != UINT32_MAX )
		{
			assert( freeRtvDescriptorCount_ < freeRtvDescriptors_.size() );
			freeRtvDescriptors_[ freeRtvDescriptorCount_++ ] = index;
		}
	}

	void DeviceManager::FreeDsvDescriptor( uint32_t index )
	{
		if( index != UINT32_MAX )
		{
			assert( freeDsvDescriptorCount_ < freeDsvDescriptors_.size() );
			freeDsvDescriptors_[ freeDsvDescriptorCount_++ ] = index;
		}
	}

	BufferResource& DeviceManager::GetBufferResource( BufferHandle handle )
	{
		BufferResource* resource = slotMapBuffers_.Get( handle );
		if( resource == nullptr )
		{
			throw std::runtime_error( "Invalid buffer handle." );
		}
		return *resource;
	}

	const BufferResource& DeviceManager::GetBufferResource( BufferHandle handle ) const
	{
		const BufferResource* resource = slotMapBuffers_.Get( handle );
		if( resource == nullptr )
		{
			throw std::runtime_error( "Invalid buffer handle." );
		}
		return *resource;
	}

	TextureResource& DeviceManager::GetTextureResource( TextureHandle handle )
	{
		TextureResource* resource = slotMapTextures_.Get( handle );
		if( resource == nullptr )
		{
			throw std::runtime_error( "Invalid texture handle." );
		}
		return *resource;
	}

	const TextureResource& DeviceManager::GetTextureResource( TextureHandle handle ) const
	{
		const TextureResource* resource = slotMapTextures_.Get( handle );
		if( resource == nullptr )
		{
			throw std::runtime_error( "Invalid texture handle." );
		}
		return *resource;
	}

	void DeviceManager::AddDeferredRelease( SubmitHandle handle, std::function<void()>&& release )
	{
		GetGraphicsQueueContext().deferredReleases_.push_back( { handle, std::move( release ) } );
	}

	void DeviceManager::ProcessDeferredReleases()
	{
		ProcessDeferredReleases( GetGraphicsQueueContext() );
	}

	void DeviceManager::ProcessDeferredReleases( QueueContext& context )
	{
		while( !context.deferredReleases_.empty() )
		{
			if( context.immediateCommands_ == nullptr || !context.immediateCommands_->IsReady( context.deferredReleases_.front().handle_ ) )
			{
				break;
			}

			context.deferredReleases_.front().release_();
			context.deferredReleases_.pop_front();
		}
	}

	void DeviceManager::WaitForQueueIdle()
	{
		WaitForQueueIdle( GetGraphicsQueueContext() );
	}

	void DeviceManager::WaitForQueueIdle( QueueContext& context )
	{
		if( context.commandQueue_ == nullptr || context.queueIdleFence_ == nullptr )
		{
			return;
		}

		context.queueIdleFenceValue_++;
		C_RESULT( context.commandQueue_->Signal( context.queueIdleFence_.Get(), context.queueIdleFenceValue_ ), "Failed to signal queue idle fence." );
		if( context.queueIdleFence_->GetCompletedValue() < context.queueIdleFenceValue_ )
		{
			C_RESULT( context.queueIdleFence_->SetEventOnCompletion( context.queueIdleFenceValue_, context.queueIdleEvent_ ),
				"Failed to wait for queue idle fence." );
			WaitForSingleObject( context.queueIdleEvent_, INFINITE );
		}
	}

	void DeviceManager::WaitIdle()
	{
		auto waitQueueIdle = [ this ]( QueueContext& context )
		{
			if( context.immediateCommands_ )
			{
				context.immediateCommands_->WaitAll();
			}

			WaitForQueueIdle( context );
			ProcessDeferredReleases( context );
			while( !context.deferredReleases_.empty() )
			{
				context.deferredReleases_.front().release_();
				context.deferredReleases_.pop_front();
			}
		};

		waitQueueIdle( graphicsQueue_ );
	}

	void DeviceManager::Shutdown() noexcept
	{
		if( graphicsQueue_.immediateCommands_ != nullptr )
		{
			graphicsQueue_.immediateCommands_->ReleaseAllCommandBuffers();
		}

		try
		{
			WaitIdle();
		}
		catch( ... )
		{
		}

		stagingDevice_.reset();
		graphicsQueue_.immediateCommands_.reset();
		baseMips_.reset();

		slotMapBuffers_.ForEach(
			[]( BufferResource& buffer )
			{
				if( buffer.mappedPtr_ != nullptr && buffer.resource_ != nullptr )
				{
					buffer.resource_->Unmap( 0, nullptr );
					buffer.mappedPtr_ = nullptr;
				}
			} );

		slotMapSwapchains_.ForEach(
			[ this ]( SwapchainResource& swapchainResource )
			{
				if( swapchainResource.swapchain_ == nullptr )
				{
					return;
				}

				const HWND hwnd = detail::GetHwnd( swapchainResource.desc_.window );
				const bool hasLiveWindow = hwnd != nullptr && IsWindow( hwnd ) != FALSE;
				IDXGISwapChain4* nativeSwapchain = swapchainResource.swapchain_->GetSwapchain();
				if( factory_ != nullptr && hasLiveWindow )
				{
					factory_->MakeWindowAssociation( hwnd, 0 );
				}
				if( nativeSwapchain != nullptr && hasLiveWindow )
				{
					BOOL isFullscreen = FALSE;
					if( SUCCEEDED( nativeSwapchain->GetFullscreenState( &isFullscreen, nullptr ) ) && isFullscreen )
					{
						nativeSwapchain->SetFullscreenState( FALSE, nullptr );
					}
				}
			} );
		slotMapSwapchains_.Clear();

		slotMapTextures_.Clear();
		slotMapBuffers_.Clear();
		slotMapSamplers_.Clear();
		graphicsQueue_.deferredReleases_.clear();

		commandSignature_.Reset();
		rootSignature_.Reset();
		dsvHeap_.Reset();
		rtvHeap_.Reset();
		samplerHeap_.Reset();
		bindlessHeap_.Reset();
		auto releaseQueueContext = []( QueueContext& context )
		{
			context.queueIdleFence_.Reset();
			if( context.queueIdleEvent_ != nullptr )
			{
				CloseHandle( context.queueIdleEvent_ );
				context.queueIdleEvent_ = nullptr;
			}

			context.commandQueue_.Reset();
		};

		releaseQueueContext( graphicsQueue_ );
		device_.Reset();
		adapter_.Reset();
		factory_.Reset();
	}

	void DeviceManager::ReportLiveObjects() noexcept
	{
#if defined( _DEBUG )
		if( !desc_.enableDebugLayer )
		{
			return;
		}

		if( device_ != nullptr )
		{
			ComPtr<ID3D12DebugDevice> debugDevice;
			if( SUCCEEDED( device_.As( &debugDevice ) ) && debugDevice != nullptr )
			{
				debugDevice->ReportLiveDeviceObjects( D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL );
			}
		}
#endif
	}

	SwapchainHandle DeviceManager::CreateSwapchainInternal( const SwapchainDesc& desc )
	{
		SwapchainResource swapchainResource{};
		swapchainResource.desc_ = desc;
		const SwapchainHandle handle = slotMapSwapchains_.Create( std::move( swapchainResource ) );
		SwapchainResource* resource = slotMapSwapchains_.Get( handle );
		if( resource == nullptr )
		{
			throw std::runtime_error( "Failed to allocate swapchain slot." );
		}

		try
		{
			resource->swapchain_ = std::make_unique<Swapchain>( *this, handle, detail::GetHwnd( desc.window ), desc.width, desc.height );
		}
		catch( ... )
		{
			slotMapSwapchains_.Destroy( handle );
			throw;
		}

		return handle;
	}

	void DeviceManager::DestroySwapchainInternal( SwapchainHandle swapchain ) noexcept
	{
		SwapchainResource* resource = slotMapSwapchains_.Get( swapchain );
		if( resource == nullptr )
		{
			return;
		}

		resource->swapchain_.reset();
		slotMapSwapchains_.Destroy( swapchain );
	}

	Swapchain* DeviceManager::GetSwapchain( SwapchainHandle swapchain ) noexcept
	{
		SwapchainResource* resource = slotMapSwapchains_.Get( swapchain );
		return resource != nullptr ? resource->swapchain_.get() : nullptr;
	}

	const Swapchain* DeviceManager::GetSwapchain( SwapchainHandle swapchain ) const noexcept
	{
		const SwapchainResource* resource = slotMapSwapchains_.Get( swapchain );
		return resource != nullptr ? resource->swapchain_.get() : nullptr;
	}

	SwapchainDesc* DeviceManager::GetSwapchainDesc( SwapchainHandle swapchain ) noexcept
	{
		SwapchainResource* resource = slotMapSwapchains_.Get( swapchain );
		return resource != nullptr ? &resource->desc_ : nullptr;
	}

	const SwapchainDesc* DeviceManager::GetSwapchainDesc( SwapchainHandle swapchain ) const noexcept
	{
		const SwapchainResource* resource = slotMapSwapchains_.Get( swapchain );
		return resource != nullptr ? &resource->desc_ : nullptr;
	}

	Swapchain* DeviceManager::GetOwningSwapchain( TextureHandle texture ) noexcept
	{
		const TextureResource* resource = slotMapTextures_.Get( texture );
		if( resource == nullptr || !resource->swapchain_.Valid() )
		{
			return nullptr;
		}

		return GetSwapchain( resource->swapchain_ );
	}

	DeviceManager::DeviceManager( const ContextDesc& desc ) : desc_( desc ), renderDevice_( *this )
	{
		try
		{
			Initialize();
		}
		catch( ... )
		{
			Shutdown();
			throw;
		}
	}

	DeviceManager& DeviceManager::Initialize( const ContextDesc& desc )
	{
		std::lock_guard lock( gDeviceManagerSingletonMutex );
		if( gDeviceManagerSingleton == nullptr )
		{
			gDeviceManagerSingleton = std::unique_ptr<DeviceManager>( new DeviceManager( desc ) );
		}
		else if( !ContextDescsAreCompatible( gDeviceManagerSingleton->desc_, desc ) )
		{
			throw std::runtime_error( "DeviceManager singleton already initialized with an incompatible ContextDesc." );
		}

		++gDeviceManagerSingletonReferenceCount;
		return *gDeviceManagerSingleton;
	}

	DeviceManager& DeviceManager::Initialize( const ContextDesc& desc, const SwapchainDesc& primarySwapchainDesc )
	{
		DeviceManager& manager = Initialize( desc );
		try
		{
			if( !manager.primarySwapchain_.Valid() )
			{
				manager.primarySwapchain_ = manager.CreateSwapchain( primarySwapchainDesc );
			}
		}
		catch( ... )
		{
			ReleaseDeviceManagerSingleton();
			throw;
		}

		return manager;
	}

	DeviceManager& DeviceManager::Get()
	{
		DeviceManager* manager = gDeviceManagerSingleton.get();
		if( manager == nullptr )
		{
			throw std::runtime_error( "DeviceManager singleton is not initialized." );
		}

		return *manager;
	}

	void DeviceManager::ShutdownSingleton()
	{
		ReleaseDeviceManagerSingleton();
	}

	DeviceManager::~DeviceManager()
	{
		Shutdown();
	}

	SwapchainHandle DeviceManager::CreateSwapchain( const SwapchainDesc& desc )
	{
		const SwapchainHandle handle = CreateSwapchainInternal( desc );
		if( !primarySwapchain_.Valid() )
		{
			primarySwapchain_ = handle;
		}

		return handle;
	}

	void DeviceManager::DestroySwapchain( SwapchainHandle swapchain )
	{
		if( !swapchain.Valid() )
		{
			return;
		}

		WaitIdle();
		DestroySwapchainInternal( swapchain );
		if( primarySwapchain_ == swapchain )
		{
			primarySwapchain_ = {};
		}
	}

	RenderDevice* DeviceManager::GetRenderDevice() noexcept
	{
		return &renderDevice_;
	}

	const RenderDevice* DeviceManager::GetRenderDevice() const noexcept
	{
		return &renderDevice_;
	}

	const DeviceProperties& DeviceManager::GetDeviceProperties() const noexcept
	{
		return deviceProperties_;
	}

	SwapchainHandle DeviceManager::RequirePrimarySwapchain() const
	{
		return primarySwapchain_;
	}

	void DeviceManager::Resize( uint32_t width, uint32_t height )
	{
		Resize( primarySwapchain_, width, height );
	}

	void DeviceManager::Resize( SwapchainHandle swapchain, uint32_t width, uint32_t height )
	{
		if( width == 0 || height == 0 )
		{
			return;
		}

		SwapchainDesc* swapchainDesc = GetSwapchainDesc( swapchain );
		Swapchain* nativeSwapchain = GetSwapchain( swapchain );
		if( swapchainDesc == nullptr || nativeSwapchain == nullptr )
		{
			return;
		}

		WaitIdle();
		swapchainDesc->width = width;
		swapchainDesc->height = height;
		nativeSwapchain->Resize( width, height );
	}

	uint32_t DeviceManager::GetWidth() const noexcept
	{
		return GetWidth( primarySwapchain_ );
	}

	uint32_t DeviceManager::GetWidth( SwapchainHandle swapchain ) const noexcept
	{
		const SwapchainDesc* swapchainDesc = GetSwapchainDesc( swapchain );
		return swapchainDesc != nullptr ? swapchainDesc->width : 0u;
	}

	uint32_t DeviceManager::GetHeight() const noexcept
	{
		return GetHeight( primarySwapchain_ );
	}

	uint32_t DeviceManager::GetHeight( SwapchainHandle swapchain ) const noexcept
	{
		const SwapchainDesc* swapchainDesc = GetSwapchainDesc( swapchain );
		return swapchainDesc != nullptr ? swapchainDesc->height : 0u;
	}

	bool DeviceManager::IsVsyncEnabled() const noexcept
	{
		return IsVsyncEnabled( primarySwapchain_ );
	}

	bool DeviceManager::IsVsyncEnabled( SwapchainHandle swapchain ) const noexcept
	{
		const SwapchainDesc* swapchainDesc = GetSwapchainDesc( swapchain );
		return swapchainDesc != nullptr ? swapchainDesc->vsync : true;
	}

	void DeviceManager::SetVsync( bool enabled ) noexcept
	{
		SetVsync( primarySwapchain_, enabled );
	}

	void DeviceManager::SetVsync( SwapchainHandle swapchain, bool enabled ) noexcept
	{
		SwapchainDesc* swapchainDesc = GetSwapchainDesc( swapchain );
		if( swapchainDesc != nullptr )
		{
			swapchainDesc->vsync = enabled;
		}
	}
}
