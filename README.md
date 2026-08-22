# LightDX12

**A small, explicit and bindless-oriented Direct3D 12 abstraction for learning, experimentation and rapid renderer prototyping.**

`LightDX12` is the repository; `LightD3D12` is the graphics library inside it. The renderer stays compact, while optional application facilities live in the separate `App` target.

## Origin

LightD3D12 started at **IFNITY** as a practical abstraction over Direct3D 12. Its first purpose was to hide the repetitive platform plumbing needed by small rendering experiments while preserving the concepts that matter: resources, pipelines, command buffers, synchronization and explicit submission.

Since then, the code has evolved into a cleaner standalone project. The API has been reduced, resource ownership has become more predictable, bindless resource access has been made central to the design, and the internal lifetime model now uses typed generational handles backed by slot maps.

The project is conceptually inspired by [corporateshark/lightweightvk](https://github.com/corporateshark/lightweightvk). LightweightVK explores a lean, modern and bindless-only API over Vulkan 1.3; LightD3D12 applies a similar spirit to Direct3D 12. It is not a fork, port or compatibility layer: both projects make backend-specific choices and expose their own APIs.

## Philosophy

LightD3D12 is built around a few deliberately simple rules:

- **Stay close to the GPU.** The API shortens Direct3D 12 setup without pretending that rendering is implicit. Applications still create resources and pipelines, record command buffers and submit work explicitly.
- **Prefer handles over exposed ownership.** Buffers, textures and swapchains cross the public API as small typed values instead of raw pointers or public COM objects.
- **Use bindless resource access as the normal path.** Shader resources live in a shared descriptor heap and can be addressed dynamically or assigned to well-known CBV, SRV and UAV slots.
- **Make the frame flow readable.** A frame should look like acquire, record, submit and present—not like a large framework lifecycle.
- **Keep experiments cheap.** Runtime HLSL compilation and compact descriptors make it quick to change shaders, pipelines and resource layouts.
- **Keep the repository focused.** The current tree contains the core library and three focused samples. It relies on the Windows SDK rather than a package manager or Git submodules.

This is intentionally not a full engine. There is no scene graph, material system, asset pipeline or editor imposed on the application. Those layers can be built above the rendering API when a project actually needs them.

## Resource handles and `SlotMap`

The public resource types are aliases of a strongly typed handle:

```cpp
using TextureHandle   = Handle<TextureResource>;
using BufferHandle    = Handle<BufferResource>;
using SwapchainHandle = Handle<SwapchainResource>;
```

Each handle is 64 bits and stores two 32-bit values:

- an **index**, used to locate the resource in its slot map;
- a **generation**, used to distinguish the current resource from an older object that previously occupied the same slot.

When a resource is destroyed, its slot is returned to a free list and its generation is incremented. This allows storage to be reused without making a recycled slot indistinguishable from an old handle, and enables stale-handle validation during development.

The slot map keeps D3D12 implementation objects inside the library while the application works with inexpensive values:

```cpp
BufferHandle buffer = device.CreateBuffer(desc);
device.WriteBuffer(buffer, 0, data, dataSize);
device.Destroy(buffer);
```

Resource handles and shader binding slots solve different problems. `BufferHandle` and `TextureHandle` identify owned objects on the CPU side; `ConstantBufferSlot`, `ShaderResourceSlot` and `ReadWriteResourceSlot` select descriptor locations visible to shaders. `CBSRVCubes` demonstrates both systems working together.

## Frame workflow

A typical frame stays explicit and compact:

```cpp
RenderDevice& device = *deviceManager.GetRenderDevice();
ICommandBuffer& commands = device.AcquireCommandBuffer();
TextureHandle backbuffer = device.GetCurrentSwapchainTexture();

commands.CmdBeginRendering(renderPass, framebuffer);
commands.CmdBindRenderPipeline(pipeline);
commands.CmdDraw(3);
commands.CmdEndRendering();

device.Submit(commands, backbuffer);
```

Under that interface, LightD3D12 manages the Direct3D 12 device, swapchains, descriptor heaps, root signature, command-list reuse, fences, resource-state transitions and deferred GPU-safe releases.

## Samples

| Sample | What it demonstrates |
| --- | --- |
| `Triangle` | Win32 setup, device and swapchain creation, runtime HLSL compilation, pipeline creation, command recording and presentation. |
| `CBSRVCubes` | Instanced procedural cubes, a structured buffer exposed through an SRV, constant-buffer data exposed through a CBV, fixed bindless slots, push constants and depth rendering. |
| `ImGuiNodeEditor` | Typed visual node graph implemented with ImGui: constants, arithmetic, `Vector3(X,Y,Z)`, links, cycle validation and live evaluation. |

The small sample count is intentional: each example is meant to explain one rendering path without bringing unrelated systems into the repository.

## Requirements

- Windows 10 or newer
- Visual Studio 2022 with **Desktop development with C++**
- Windows 10/11 SDK with DXC (`dxcompiler.dll` and `dxil.dll`)
- CMake 3.24 or newer available in `PATH`

## Generate and build

Double-click `GenerateSolution.bat`, or run it from a terminal:

```bat
GenerateSolution.bat
```

The script creates:

```text
build\LightDX12.sln
```

The generated solution contains the renderer, application layer, and sample targets. `Triangle` is the startup project. You can also build from the command line:

```bat
cmake --build build --config Debug --parallel
```

## Use only the static library

`LightD3D12` is a static library. Application support, examples, tests and installation rules are controlled independently:

| Option | Top-level default | Subproject default |
| --- | --- | --- |
| `LIGHTDX12_BUILD_APP` | `ON` | `OFF` |
| `LIGHTDX12_BUILD_EXAMPLES` | `ON` | `OFF` |
| `LIGHTDX12_BUILD_TESTS` | `ON` | `OFF` |
| `LIGHTDX12_INSTALL` | `ON` | `OFF` |

When the repository is included from another CMake project, only the core library is generated by default:

```cmake
add_subdirectory(external/LightDX12 EXCLUDE_FROM_ALL)
target_link_libraries(MyApplication PRIVATE LightD3D12::LightD3D12)
```

The same target works with `FetchContent`:

```cmake
include(FetchContent)
FetchContent_Declare(
    LightDX12
    GIT_REPOSITORY https://github.com/alfonsmagd/LightDX12.git
    GIT_TAG <tag-or-commit>
)
FetchContent_MakeAvailable(LightDX12)

target_link_libraries(MyApplication PRIVATE LightD3D12::LightD3D12)
```

To build and install a reusable package containing only the static library and its public headers:

```bat
cmake -S . -B build-core ^
    -DLIGHTDX12_BUILD_APP=OFF ^
    -DLIGHTDX12_BUILD_EXAMPLES=OFF ^
    -DLIGHTDX12_BUILD_TESTS=OFF ^
    -DLIGHTDX12_INSTALL=ON
cmake --build build-core --config Release --target LightD3D12 --parallel
cmake --install build-core --config Release --prefix install
```

Another CMake project can then consume that installation:

```bat
cmake -S . -B build -DCMAKE_PREFIX_PATH=C:\path\to\LightDX12\install
```

```cmake
find_package(LightD3D12 CONFIG REQUIRED)
target_link_libraries(MyApplication PRIVATE LightD3D12::LightD3D12)
```

## Repository layout

```text
LightDX12/
|-- App/                 Optional ImGui and node-graph support
|-- LightD3D12/          Core library, public headers and internal shaders
|-- samples/
|   |-- Triangle/        Minimal draw-and-present path
|   |-- CBSRVCubes/      Bindless CBV/SRV resource example
|   `-- ImGuiNodeEditor/ Typed visual node graph example
|-- third_party/imgui/   Dear ImGui core and Win32/DX12 backends
|-- CMakeLists.txt
`-- GenerateSolution.bat
```

## Current scope

LightD3D12 is a compact, evolving renderer abstraction rather than a compatibility promise or finished general-purpose engine. Its goal is to keep modern Direct3D 12 experiments understandable: explicit enough to teach the underlying model, small enough to change without fighting a framework.
