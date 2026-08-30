![Ldx12](https://raw.githubusercontent.com/alfonsmagd/LightDX12/v0.2.0/nuget/Ldx12.png)

# Ldx12 0.2.0

**Lightweight. Fully bindless.**

Ldx12 is a compact C++20 Direct3D 12 library for prototyping modern renderers quickly, with a small API and low CPU overhead.

## Included

- `Ldx12`: device, swapchain, resources, pipelines, command buffers and GPU submission.
- `Ldx12Utils`: optional helpers for application setup, cameras, geometry, depth targets and texture loading.
- Native x64 static libraries for Debug and Release.
- Public headers for both libraries.

The package configures include paths and selects the correct libraries automatically in Visual Studio 2022.

## Full bindless layout

Resources and samplers are accessed by index from shaders. Ldx12 owns the descriptor heaps and exposes small typed handles to the application.

![Ldx12 bindless layout and root signature](https://raw.githubusercontent.com/alfonsmagd/LightDX12/v0.2.0/Ldx12/docs/images/bindless-layout.png)

## Minimal use

```cpp
#include <Ldx12/Ldx12.hpp>

using namespace ldx12;

DeviceManager& manager = DeviceManager::Initialize( context, swapchain );
RenderDevice& device = *manager.GetRenderDevice();
```

Utilities are available through:

```cpp
#include <Ldx12Utils/Ldx12Utils.hpp>
```

## Requirements

- Windows 10 or 11.
- Visual Studio 2022, C++20 and an x64 target.
- Direct3D 12 feature level 12.0, Shader Model 6.6 and Resource Binding Tier 2.

Source, samples and documentation: [github.com/alfonsmagd/LightDX12](https://github.com/alfonsmagd/LightDX12)
