# Ldx12 technical reference

See the [README](../../README.md) for getting started and the [public header](../include/Ldx12/Ldx12.hpp) for API declarations.

## Command buffer lifetime

`AcquireCommandBuffer()` reserves a command buffer until it is submitted or discarded. Call `Discard()` when an acquired recording will not be submitted; this immediately returns its slot to the pool and drops its pending resource-state tracking. End an active render pass before submitting or discarding. A command-buffer reference is invalid after either operation.

## Pipeline color formats

Create the render-target texture first, then use its actual format when creating the pipeline. This keeps the texture as the source of truth.

```cpp
TextureDesc targetDesc{};
targetDesc.width = width;
targetDesc.height = height;
targetDesc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
targetDesc.usage = TextureUsage::RenderTarget;

TextureHandle target = device.CreateTexture( targetDesc );

RenderPipelineDesc pipelineDesc{};
pipelineDesc.color[ 0 ].format = device.GetTextureFormat( target );
```

For MRT, query every texture in framebuffer order:

```cpp
pipelineDesc.color[ 0 ].format = device.GetTextureFormat( albedo );
pipelineDesc.color[ 1 ].format = device.GetTextureFormat( normals );
pipelineDesc.color[ 2 ].format = device.GetTextureFormat( objectId );
```

When the D3D12 debug layer is enabled, binding a render pipeline reports debugger warnings if its color formats differ from the active framebuffer. `DXGI_FORMAT_UNKNOWN` also makes missing or additional color attachments visible through the same comparison. The warning does not interrupt command recording.

`color[0].format` defaults to `DXGI_FORMAT_R8G8B8A8_UNORM`. For a depth-only pipeline, set it to `DXGI_FORMAT_UNKNOWN`.

`RenderPipelineDesc::colorFormat` remains as a deprecated single-RTV compatibility fallback in `0.4.0`. Explicit `color[i].format` configuration takes precedence.

## Build options

Defaults for a fresh CMake configuration:

| Option | Top-level | As a subproject |
| --- | --- | --- |
| `LDX12_BUILD_APP` | `ON` | `OFF` |
| `LDX12_BUILD_EXAMPLES` | `ON` | `OFF` |
| `LDX12_BUILD_TESTS` | `ON` | `OFF` |
| `LDX12_BUILD_UTILS` | `OFF` | `OFF` |
| `LDX12_BUILD_THIRD_PARTY` | `ON` | `OFF` |
| `LDX12_INSTALL` | `ON` | `OFF` |

Examples also build Utils and ImGui support. Set `LDX12_BUILD_UTILS=ON` to include Utils in the installed package. `LDX12_BUILD_THIRD_PARTY=OFF` omits cgltf, the glTF loader and related samples/tests; it does not disable ImGui.

`LDX12_BUILD_DESKTOP_RETRO_OVERLAY` independently controls the desktop overlay and defaults to the value of `LDX12_BUILD_EXAMPLES`.

See [CMake organization](../../cmake/README.md) for target layout.

## Capacities

These are Ldx12 software capacities, not resources allocated immediately by the GPU.

Resource storage is a fixed contiguous array sized at compile time. Select one profile for both the library and its consumers:

| Profile define | Buffers + textures | Shared CBV/SRV/UAV | RTV | DSV | Tracked buffers/textures per command buffer |
| --- | ---: | ---: | ---: | ---: | ---: |
| `LDX12_SMALL_RESOURCES` | 256 | 256 | 16 | 4 | 16 per type |
| `LDX12_MEDIUM_RESOURCES` | 1,024 | 1,024 | 32 | 8 | 64 per type |
| `LDX12_HIGH_RESOURCES` (default) | 4,096 | 4,096 | 64 | 12 | 256 per type |

With CMake, configure `-DLDX12_RESOURCE_PROFILE=SMALL`, `MEDIUM` or `HIGH`. The `Ldx12::Ldx12` target propagates the selected define, including installed packages. For manual builds, use the same profile define in every library and application translation unit. Selecting more than one profile is a compilation error.

CBV, SRV and UAV descriptors share one heap. Index 0 is invalid, indices 1-25 are predefined and the dynamic portion starts at index 26: 230 slots for SMALL, 998 for MEDIUM and 4,070 for HIGH. `ContextDesc` can reduce heap capacities, but cannot exceed the compiled profile.

Buffers and textures share the resource limit. Imported textures and swapchain backbuffers also count. A sampled render target uses one resource slot, one SRV and one RTV; descriptor limits count all allocated views, including views retained for deferred GPU cleanup. All CPU resource and descriptor bookkeeping arrays have static storage; there are no resource pages.

The following maxima describe the default HIGH profile:

| API capacity | Maximum |
| --- | ---: |
| Live buffers and textures | 4,096 resources combined |
| Live swapchains | 16 swapchains |
| Backbuffers per swapchain | 2-3 backbuffers |
| Color attachments per render pass | 8 attachments |
| Vertex input elements per pipeline | 16 elements |
| Acquired command buffers awaiting submission | 64 command buffers |
| Command buffers per submission batch | 4 command buffers |
| Texture states tracked per command buffer | 256 textures |
| Push constants | 62 × 32-bit values (248 bytes) |

The predefined descriptor positions are conveniences, not resource limits. Applications can use five CBV, five SRV and three UAV predefined positions; other resources receive dynamic descriptor indices.

Engine slots 6 and 7 are named `EngineRingBuffer0` and `EngineRingBuffer1`. Slots 8-10 remain `EngineObject`, `EngineMaterial` and `EngineLighting`. These names identify fixed descriptor positions; they do not create buffers automatically.

The root signature uses all 64 DWORDs: 62 application constants at `b0`, the existing indirect draw index at `b1`, and one reserved constant at `b2`. The `b2` value packs the ring 0 byte offset in its low 16 bits and the ring 1 byte offset in its high 16 bits. Both offsets are initialized to zero for graphics and compute when a command buffer is acquired. `CmdPushConstants` only writes `b0` and cannot overwrite the reserved offsets.

## Platform and scope

### Migrating to 0.4.0

- Rebuild applications and shaders against the same version and resource profile. Buffers and textures now share the profile's resource capacity.
- Application push constants at `b0` are limited to 62 DWORDs (248 bytes). `b1` is reserved for indirect drawing and `b2` for constant-buffer ring offsets.
- `SetConstantBuffer` copies data into a GPU-safe ring allocation. Include `Ldx12_ConstantBuffers.hlsli` to read it through `CB0.Load<T>()` and `CB1.Load<T>()`.
- CMake installations and NuGet packages place the HLSL headers in `include/Ldx12/shaders` and `build/native/include/Ldx12/shaders`, respectively. Add that directory to `ShaderStageSource::includeDirectories` when compiling consumer shaders.
- Buffer-descriptor preconditions use assertions; callers must satisfy them in Release builds as well. Resource creation and capacity failures retain their runtime errors.
- Software adapters, including WARP, are unsupported. Initialization requires a compatible hardware adapter.

Initialization requires a D3D12 device supporting Shader Model 6.6. `ContextDesc::minimumFeatureLevel` is passed directly to device creation and defaults to 12.0. No additional feature-level or resource-binding-tier queries are performed during adapter selection.

### GPU selection and capability checks

Use `DeviceManager::Initialize(contextDesc)` to select and create a compatible device, then `GetDeviceProperties()` to inspect its name, LUID, memory and Shader Model. There is no separate public adapter-probing API.

```cpp
ContextDesc contextDesc{};
contextDesc.adapterIndex = 1;

DeviceManager& manager = DeviceManager::Initialize( contextDesc );
const DeviceProperties& properties = manager.GetDeviceProperties();

std::cout << properties.adapterName << '\n';
```

The selection defaults can be changed in `Ldx12_Defines.hpp`, or overridden with compiler defines shared by the library and every consumer:

```cpp
#define LDX12_PREFERRED_GPU_VENDOR LDX12_GPU_VENDOR_NVIDIA
#define LDX12_PREFERRED_GPU_INDEX UINT32_MAX
```

Use `LDX12_GPU_VENDOR_DEFAULT`, `LDX12_GPU_VENDOR_NVIDIA` or `LDX12_GPU_VENDOR_AMD`. With `UINT32_MAX` as the index, a vendor preference selects the first compatible GPU from that vendor. If none is available, selection falls back to the first compatible hardware adapter in DXGI's `EnumAdapters1` order.

To choose a specific GPU when several are installed, set `LDX12_PREFERRED_GPU_INDEX` to its `EnumAdapters1` index (for example `1u`). The index takes priority over the vendor flag. If that adapter is unavailable or incompatible, selection falls back to the first compatible hardware adapter in DXGI's default order. The index is global, not relative to a vendor's filtered list.

`ContextDesc::preferredAdapterVendor` and `ContextDesc::adapterIndex` use these defines as defaults and can also be assigned at runtime. With no vendor or index preference, the existing `preferHighPerformanceAdapter` setting controls whether automatic selection follows DXGI's high-performance order (default) or `EnumAdapters1` order.

Every candidate must support SM 6.6 before engine resources are created. If none passes, initialization throws `std::runtime_error` with the last rejection reason, or a generic SM 6.6 requirement message when no candidate was found. Indices may change after hardware changes, so enumerate again instead of persisting them across runs. `GetDeviceProperties().adapterLuid` identifies the selected adapter in the current Windows session.

Software adapters are always excluded, including when explicitly selected by index. WARP is not supported. If no compatible hardware adapter is available, initialization throws.

`InitializeDevice()` explicitly creates the candidate device and retains it when SM 6.6 is supported. `ProbeAdapter(ID3D12Device*)` only queries support on an existing device. `GetAdapterProperties()` copies adapter metadata from the descriptor already read by the caller.

The probe makes one [Shader Model capability query](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ns-d3d12-d3d12_feature_data_shader_model) for 6.6. A failed query or a returned version below 6.6 rejects the candidate. `RenderDevice::BindlessSupported()` queries resource binding support only when explicitly called.

- Swapchains use Win32 `HWND`.
- Compute and graphics use the graphics queue; there is no dedicated compute queue.
- Binding is bindless-only; traditional per-draw descriptor tables are not provided.
- Ray tracing, mesh shaders and amplification shaders are not provided.
- Texture data can be supplied at creation; the public API does not yet expose runtime CPU texture updates.

For native interoperability, include `Ldx12Native.hpp` and use `GetNative()`. Returned native pointers are borrowed.

## glTF examples

[18_GltfScene](../../samples/GltfScene) and [19_DrawIndirectGltf](../../samples/DrawIndirectGltf) share loading and material support through `GltfSampleCommon`.

The first issues a `CmdDrawIndexed` per primitive. The second packs geometry into shared buffers and submits it through `CmdDrawIndexedIndirect`.

## Desktop retro overlay

[DesktopRetroOverlay](../../samples/thirdParty/DesktopRetroOverlay) contains separate capture, effect-rendering and control modules. Capture uses D3D11 and Windows Graphics Capture; effects run through Ldx12/D3D12.

Enable `LDX12_BUILD_DESKTOP_RETRO_OVERLAY` and build the `DesktopRetroOverlay` target. Presets include CRTV, amber, green, PS2 Clean and NewPixie.
