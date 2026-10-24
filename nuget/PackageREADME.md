![Ldx12](https://raw.githubusercontent.com/alfonsmagd/LightDX12/v0.4.0/nuget/Ldx12Banner.png)

# Ldx12 0.4.0

**Prototype Direct3D 12 bindless renderers quickly with a compact C++20 API.**

Ldx12 removes repetitive Direct3D 12 setup while keeping rendering explicit. It manages the device, swapchain, descriptor heaps, bindless root signature, command-list reuse, fences, resource states and deferred GPU-safe destruction.

It is intended for graphics experiments, tools and renderer prototypes rather than as a complete game engine.

## Install

From the Visual Studio Package Manager Console:

```powershell
Install-Package Ldx12 -Version 0.4.0
```

The package configures include paths automatically and selects the correct x64 static libraries for Debug or Release.

## A frame at a glance

With a device and render pipeline already initialized:

```cpp
TextureHandle backbuffer = device.GetCurrentSwapchainTexture();

RenderPass renderPass{};
renderPass.color[ 0 ].loadOp = LoadOp::Clear;
renderPass.color[ 0 ].clearColor = { 0.03f, 0.04f, 0.08f, 1.0f };

Framebuffer framebuffer{};
framebuffer.color[ 0 ].texture = backbuffer;

CommandBuffer& commands = device.AcquireCommandBuffer();
commands.CmdBeginRendering( renderPass, framebuffer );
commands.CmdBindRenderPipeline( pipeline );
commands.CmdDraw( 3 );
commands.CmdEndRendering();

device.Submit( commands, backbuffer );
```

## Bindless: pass indices instead of binding tables

In native D3D12, changing a texture and buffer commonly means preparing compatible descriptor tables and binding them to matching root parameters:

```cpp
commandList->SetGraphicsRootDescriptorTable( 0, textureDescriptor );
commandList->SetGraphicsRootDescriptorTable( 1, bufferDescriptor );
```

With Ldx12, resources already live in a shared descriptor heap. Pass their indices through push constants:

```cpp
struct Resources
{
    uint32_t textureIndex;
    uint32_t bufferIndex;
};

const Resources resources = {
    device.GetBindlessIndex( texture ),
    device.GetBindlessIndex( buffer )
};

commands.CmdPushConstants( resources );
```

The shader accesses the resources directly:

```hlsl
cbuffer Resources : register(b0)
{
    uint textureIndex;
    uint bufferIndex;
};

float4 PSMain(float2 uv : TEXCOORD0) : SV_Target0
{
    Texture2D<float4> texture = ResourceDescriptorHeap[textureIndex];
    StructuredBuffer<float4> colors = ResourceDescriptorHeap[bufferIndex];
    SamplerState linearClamp = SamplerDescriptorHeap[0];

    return texture.Sample(linearClamp, uv) * colors[0];
}
```

Changing a resource means changing an index. Ldx12 owns the heaps and bindless root signature; the application still controls pipelines, commands and synchronization points.

![Ldx12 bindless layout and root signature](https://raw.githubusercontent.com/alfonsmagd/LightDX12/v0.4.0/Ldx12/docs/images/bindless-layout.png)

## Multiple render targets

Each pipeline color attachment declares its own format. The formats can be obtained directly from the framebuffer textures:

```cpp
RenderPipelineDesc pipelineDesc{};
pipelineDesc.color[ 0 ].format = device.GetTextureFormat( colorTexture );
pipelineDesc.color[ 1 ].format = device.GetTextureFormat( normalTexture );
pipelineDesc.color[ 2 ].format = device.GetTextureFormat( materialTexture );

Framebuffer framebuffer{};
framebuffer.color[ 0 ].texture = colorTexture;
framebuffer.color[ 1 ].texture = normalTexture;
framebuffer.color[ 2 ].texture = materialTexture;
```

When a pipeline is bound, Ldx12 reports a debugger warning if its attachment formats do not match the active framebuffer.

## Load glTF and GLB scenes with Ldx12Utils

The bundled `Ldx12Utils` library provides a static glTF 2.0 scene loader powered by the vendored [cgltf](https://github.com/jkuhlmann/cgltf) parser. Loading is CPU-only and does not require a D3D12 device:

```cpp
#include <Ldx12Utils/GltfLoader.hpp>

ldx12::utils::GltfScene scene =
    ldx12::utils::LoadGltfScene( "scene.glb" );

for( const ldx12::utils::GltfPrimitive& primitive : scene.primitives )
{
    // Upload primitive.vertices, primitive.indices and its material textures.
}
```

The loader supports `.gltf` and `.glb`, scene-node transforms, indexed and non-indexed triangle geometry, generated normals, metallic/roughness materials, external or embedded images and per-texture glTF samplers. Node transforms are baked into vertices and converted to left-handed coordinates.

Complete examples are available in the repository:

- [18_GltfScene](https://github.com/alfonsmagd/LightDX12/tree/v0.4.0/samples/GltfScene) uploads a complete scene and renders its material textures with GGX PBR, HDR environment lighting and optional MSAA.
- [19_DrawIndirectGltf](https://github.com/alfonsmagd/LightDX12/tree/v0.4.0/samples/DrawIndirectGltf) loads glTF scenes and renders their primitives through bindless material buffers and indexed indirect draws.

The loader intentionally targets static demo scenes; animation, skinning, compression, alpha blending and glTF extensions are outside its current scope.

## What is new in 0.4.0

- GPU adapter selection by DXGI index or NVIDIA/AMD preference, with fallback to compatible hardware.
- SMALL, MEDIUM and HIGH resource profiles with fixed capacities shared by buffers and textures.
- GPU-safe per-draw constant-buffer rings through `SetConstantBuffer`, with typed `CB0.Load<T>()` / `CB1.Load<T>()` shader access.
- CPU initial-data uploads for `Texture3D`, including padded rows and depth slices.
- TriplanarMapping and ByteAddressBuffer samples, a skybox in DrawIndirectGltf and a depth toggle in DepthPass.
- Corrected `SlotMap` move behavior and tests that remain active in Release.

Application push constants now support **62 DWORDs (248 bytes)** at `b0`. Root constant `b2` is reserved for ring-buffer offsets. Rebuild applications and shaders against the same Ldx12 version and resource profile.

The public shader headers are installed in `build/native/include/Ldx12/shaders`. Add that directory to `ShaderStageSource::includeDirectories`, then include `Ldx12_ConstantBuffers.hlsli` from your shader. Both headers needed by the ring-buffer interface are included in the package.

## Included in the package

- `Ldx12`: device, swapchains, resources, pipelines, command buffers and submission.
- `Ldx12Utils`: Win32 application setup, cameras, geometry, depth targets, texture loading and glTF loading.
- Public C++ and HLSL headers and native x64 static libraries for Debug and Release.
- MSBuild integration for Visual Studio 2022.
- MIT license and third-party notices.

Normal rendering uses:

```cpp
#include <Ldx12/Ldx12.hpp>
```

Optional utilities use:

```cpp
#include <Ldx12Utils/Ldx12Utils.hpp>
```

Advanced integrations can include `Ldx12Native.hpp` to access borrowed native D3D12 objects.

## Requirements

- Windows 10 or Windows 11, x64.
- Visual Studio 2022 with C++20 support.
- Windows SDK and DXC for runtime shader compilation.
- Direct3D feature level 12.0 or newer.
- Shader Model 6.6.
- A compatible hardware GPU; software adapters such as WARP are unsupported.

## Learn more

- [Source and full documentation](https://github.com/alfonsmagd/LightDX12)
- [Samples](https://github.com/alfonsmagd/LightDX12/tree/v0.4.0/samples)
- [Changelog](https://github.com/alfonsmagd/LightDX12/blob/v0.4.0/CHANGELOG.md)
- [Third-party notices](https://github.com/alfonsmagd/LightDX12/blob/v0.4.0/THIRD_PARTY_NOTICES.md)
