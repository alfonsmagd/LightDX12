![Ldx12](https://raw.githubusercontent.com/alfonsmagd/LightDX12/v0.4.0/nuget/Ldx12Banner.png)

# Ldx12 0.4.0

C++20 Direct3D 12 library with bindless resources, explicit rendering and compute.

## Install

```powershell
Install-Package Ldx12 -Version 0.4.0
```

Includes Ldx12, Ldx12Utils and x64 Debug/Release libraries. Requires Visual Studio 2022, the Windows SDK with DXC and a hardware GPU supporting Shader Model 6.6.

Snippets use an initialized `RenderDevice& device` and `using namespace ldx12;`.

## Load a texture

```cpp
#include <Ldx12Utils/TextureLoader.hpp>

const utils::ImageRgba8 image = utils::LoadImageRgba8( "albedo.png" );

TextureDesc textureDesc{};
textureDesc.width = image.width;
textureDesc.height = image.height;
textureDesc.data = image.pixels.data();
textureDesc.rowPitch = image.width * 4u;
textureDesc.slicePitch = textureDesc.rowPitch * image.height;

const TextureHandle texture = device.CreateTexture( textureDesc );
```

Pass its index before drawing:

```cpp
commands.CmdPushConstants( device.GetBindlessIndex( texture ) );
```

Read it in your pixel shader:

```hlsl
cbuffer Bindings : register(b0)
{
    uint textureIndex;
};

float4 PSMain(float2 uv : TEXCOORD0) : SV_Target0
{
    Texture2D<float4> texture = ResourceDescriptorHeap[textureIndex];
    SamplerState linearClamp = SamplerDescriptorHeap[0];

    return texture.Sample(linearClamp, uv);
}
```

## Create a GPU buffer

```cpp
constexpr uint32_t count = 1024;

BufferDesc bufferDesc{};
bufferDesc.type = BufferType::Structured;
bufferDesc.size = count * sizeof( float );
bufferDesc.stride = sizeof( float );
bufferDesc.unorderedAccess = true;

const BufferHandle buffer = device.CreateBuffer( bufferDesc );
```

## Run compute

`Compute.hlsl` writes one value per buffer element:

```hlsl
cbuffer Bindings : register(b0)
{
    uint outputIndex;
};

[numthreads(64, 1, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    RWStructuredBuffer<float> output = ResourceDescriptorHeap[outputIndex];
    output[id.x] = float(id.x);
}
```

```cpp
#include <Ldx12/HLSLLoader.hpp>

ComputePipelineDesc pipelineDesc{};
pipelineDesc.computeShader = HLSLLoader::LoadStage( "Compute.hlsl", "cs_6_6", "CSMain" );

ComputePipelineState pipeline = device.CreateComputePipeline( pipelineDesc );

CommandBuffer& commands = device.AcquireCommandBuffer();
commands.CmdTransitionBuffer( buffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS );
commands.CmdBindComputePipeline( pipeline );
commands.CmdPushConstants( device.GetUnorderedAccessIndex( buffer ) );
commands.CmdDispatch( count / 64u );
commands.CmdTransitionBuffer( buffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE );
device.Submit( commands );
```

The buffer is ready for a subsequent GPU shader read. Keep the pipeline alive until its submission completes.

## Constant data

```cpp
commands.SetConstantBuffer( constants );
```

Include `Ldx12_ConstantBuffers.hlsli` and read `CB0.Load<YourType>()` in HLSL. Add `build/native/include/Ldx12/shaders` from the package to the stage's `includeDirectories`.

Application push constants: **248 bytes** at `b0`. Registers `b1` and `b2` are reserved.

[Samples](https://github.com/alfonsmagd/LightDX12/tree/v0.4.0/samples) · [API reference](https://github.com/alfonsmagd/LightDX12/blob/v0.4.0/Ldx12/docs/REFERENCE.md) · [Changelog](https://github.com/alfonsmagd/LightDX12/blob/v0.4.0/CHANGELOG.md)
