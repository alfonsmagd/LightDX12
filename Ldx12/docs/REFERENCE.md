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

`RenderPipelineDesc::colorFormat` remains as a deprecated single-RTV compatibility fallback for `0.3.x` and will be removed in `0.4.0`. Explicit `color[i].format` configuration takes precedence.

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

| Descriptor heap | Default | Maximum |
| --- | ---: | ---: |
| Shared CBV/SRV/UAV | 4,096 descriptors | 4,096 descriptors |
| Dynamic CBV/SRV/UAV portion | 4,070 descriptors | 4,070 descriptors |
| RTV | 256 descriptors | 256 descriptors |
| DSV | 64 descriptors | 64 descriptors |

CBV, SRV and UAV descriptors share one heap. Index 0 is invalid, indices 1-25 are predefined and indices 26-4095 are dynamic. `ContextDesc` can reduce these capacities.

| API capacity | Maximum |
| --- | ---: |
| Live buffers | 4,096 buffers |
| Live textures | 4,096 textures |
| Live swapchains | 16 swapchains |
| Backbuffers per swapchain | 2-3 backbuffers |
| Color attachments per render pass | 8 attachments |
| Vertex input elements per pipeline | 16 elements |
| Acquired command buffers awaiting submission | 64 command buffers |
| Command buffers per submission batch | 4 command buffers |
| Texture states tracked per command buffer | 256 textures |
| Push constants | 63 × 32-bit values (252 bytes) |

The predefined descriptor positions are conveniences, not resource limits. Applications can use five CBV, five SRV and three UAV predefined positions; other resources receive dynamic descriptor indices.

## Platform and scope

The current core requires Windows, Direct3D feature level 12.0, Shader Model 6.6 and Resource Binding Tier 3.

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
