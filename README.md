# Lightdx12

**A small, explicit and bindless-oriented Direct3D 12 abstraction for learning, experimentation and rapid renderer prototyping.**

`Ldx12` is the graphics library and the public name used throughout its API, CMake package and examples. Optional application facilities live in the separate `App` target.

## Origin

Ldx12 started at **IFNITY** as a practical abstraction over Direct3D 12. Its first purpose was to hide the repetitive platform plumbing needed by small rendering experiments while preserving the concepts that matter: resources, pipelines, command buffers, synchronization and explicit submission.

Since then, the code has evolved into a cleaner standalone project. The API has been reduced, resource ownership has become more predictable, bindless resource access has been made central to the design, and the internal lifetime model now uses typed generational handles backed by slot maps.

The project is conceptually inspired by [corporateshark/lightweightvk](https://github.com/corporateshark/lightweightvk). LightweightVK explores a lean, modern and bindless-only API over Vulkan 1.3; Ldx12 applies a similar spirit to Direct3D 12. It is not a fork, port or compatibility layer: both projects make backend-specific choices and expose their own APIs.

## Philosophy

Ldx12 is built around a few deliberately simple rules:

- **Stay close to the GPU.** The API shortens Direct3D 12 setup without pretending that rendering is implicit. Applications still create resources and pipelines, record command buffers and submit work explicitly.
- **Prefer handles over exposed ownership.** Buffers, textures and swapchains cross the public API as small typed values instead of raw pointers or public COM objects.
- **Use bindless resource access as the normal path.** Shader resources live in a shared descriptor heap and can be addressed dynamically or assigned to well-known CBV, SRV and UAV slots.
- **Make the frame flow readable.** A frame should look like acquire, record, submit and present—not like a large framework lifecycle.
- **Keep experiments cheap.** Runtime HLSL compilation and compact descriptors make it quick to change shaders, pipelines and resource layouts.
- **Keep the repository focused.** The current tree contains the core library and four focused samples. It relies on the Windows SDK rather than a package manager or Git submodules.

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

Under that interface, Ldx12 manages the Direct3D 12 device, swapchains, descriptor heaps, root signature, command-list reuse, fences, resource-state transitions and deferred GPU-safe releases.

The complete lifetime of a single triangle frame remains compact (`CreateApplicationWindow` and `CreateTrianglePipeline` are the platform and shader helpers shown in the full [`Triangle`](samples/Triangle/main.cpp) sample):

```cpp
HWND hwnd = CreateApplicationWindow();

SwapchainDesc swapchain{
    MakeWin32WindowHandle( hwnd ),
    1280,
    720,
    true
};

DeviceManager& manager = DeviceManager::Initialize( {}, swapchain );
RenderDevice& device = *manager.GetRenderDevice();
RenderPipelineState pipeline = CreateTrianglePipeline( device );

TextureHandle backbuffer = device.GetCurrentSwapchainTexture();
Framebuffer framebuffer{};
framebuffer.color[ 0 ].texture = backbuffer;

ICommandBuffer& commands = device.AcquireCommandBuffer();
commands.CmdBeginRendering( {}, framebuffer );
commands.CmdBindRenderPipeline( pipeline );
commands.CmdDraw( 3 );
commands.CmdEndRendering();

device.Submit( commands, backbuffer );
device.WaitIdle();
DeviceManager::ShutdownSingleton();
```

## Samples

| Sample | What it demonstrates |
| --- | --- |
| `Triangle` | Win32 setup, device and swapchain creation, runtime HLSL compilation, pipeline creation, command recording and presentation. |
| `ViewportScissor` | Three viewport regions on the left and a visibly clipped scissor region on the right, without an application framework. |
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
build\Ldx12.sln
```

The generated solution contains the renderer, application layer, and sample targets. `Triangle` is the startup project. You can also build from the command line:

```bat
cmake --build build --config Debug --parallel
```

## Use only the static library

`Ldx12` is a static library. Application support, examples, tests and installation rules are controlled independently:

| Option | Top-level default | Subproject default |
| --- | --- | --- |
| `LDX12_BUILD_APP` | `ON` | `OFF` |
| `LDX12_BUILD_EXAMPLES` | `ON` | `OFF` |
| `LDX12_BUILD_TESTS` | `ON` | `OFF` |
| `LDX12_INSTALL` | `ON` | `OFF` |

When the repository is included from another CMake project, only the core library is generated by default:

```cmake
add_subdirectory(external/Ldx12 EXCLUDE_FROM_ALL)
target_link_libraries(MyApplication PRIVATE Ldx12::Ldx12)
```

The same target works with `FetchContent`:

```cmake
include(FetchContent)
FetchContent_Declare(
    Ldx12
    GIT_REPOSITORY https://github.com/alfonsmagd/LightDX12.git
    GIT_TAG <tag-or-commit>
)
FetchContent_MakeAvailable(Ldx12)

target_link_libraries(MyApplication PRIVATE Ldx12::Ldx12)
```

To build and install a reusable package containing only the static library and its public headers:

```bat
cmake -S . -B build-core ^
    -DLDX12_BUILD_APP=OFF ^
    -DLDX12_BUILD_EXAMPLES=OFF ^
    -DLDX12_BUILD_TESTS=OFF ^
    -DLDX12_INSTALL=ON
cmake --build build-core --config Release --target Ldx12 --parallel
cmake --install build-core --config Release --prefix install
```

Another CMake project can then consume that installation:

```bat
cmake -S . -B build -DCMAKE_PREFIX_PATH=C:\path\to\Ldx12\install
```

```cmake
find_package(Ldx12 CONFIG REQUIRED)
target_link_libraries(MyApplication PRIVATE Ldx12::Ldx12)
```

A complete independent consumer with its own [`main.cpp`](examples/InstalledLdx12/main.cpp) is available in [`examples/InstalledLdx12`](examples/InstalledLdx12/README.md). Set `LDX12_USE_FETCHCONTENT=OFF` to consume an installed package or `ON` to download Ldx12 automatically.

## Repository layout

```text
Ldx12/
|-- App/                 Optional ImGui and node-graph support
|-- Ldx12/               Core library, public headers and internal shaders
|-- samples/
|   |-- Triangle/        Minimal draw-and-present path
|   |-- ViewportScissor/ Explicit viewport and scissor regions
|   |-- CBSRVCubes/      Bindless CBV/SRV resource example
|   `-- ImGuiNodeEditor/ Typed visual node graph example
|-- examples/
|   `-- InstalledLdx12/  Independent find_package consumer
|-- third_party/imgui/   Dear ImGui core and Win32/DX12 backends
|-- CMakeLists.txt
`-- GenerateSolution.bat
```

## Current scope

Ldx12 is a compact, evolving renderer abstraction rather than a compatibility promise or finished general-purpose engine. Its goal is to keep modern Direct3D 12 experiments understandable: explicit enough to teach the underlying model, small enough to change without fighting a framework.

## Tests

Test descriptions and commands are available in the [tests documentation](tests/README.md).

## Supported scope

| Area | Current support |
| --- | --- |
| Platform and backend | Windows 10/11 with Direct3D 12, feature level 12.0 or newer. |
| Distribution | C++20 static library through `add_subdirectory`, `FetchContent` or an installed CMake package. |
| Windows and presentation | Win32 `HWND` swapchains, up to three backbuffers, VSync and tearing configuration. |
| Graphics shaders | Runtime DXC compilation of Shader Model 6.6 vertex and pixel shaders. |
| Graphics pipelines | Color/depth attachments, viewport and scissor control, vertex and index buffers, push constants, draw, indexed draw, instancing and indirect indexed draw. |
| Resources | Typed generational handles, buffers, 2D/3D textures, uploads, 2D texture downloads and external D3D12 texture import. |
| Binding | Shared bindless CBV/SRV/UAV heap with fixed and dynamic slots, plus RTV and DSV descriptors. |
| Commands and synchronization | Reusable graphics command buffers, submissions, batches of up to four command buffers, fences, waits and deferred GPU-safe destruction. |
| Diagnostics | D3D12 debug layer, scoped GPU labels and optional PIX capture attachment. |

The following paths are deliberately **not directly supported**:

- **Dedicated compute shaders:** there is no public compute command-buffer, compute-queue or compute-submit path. The current low-level `CreateComputePipeline`, `CmdBindComputePipeline` and `CmdDispatch` primitives run inside the graphics command-buffer path; they are not a complete independent compute API.
- **Ray tracing:** there is no DXR pipeline, acceleration-structure management, shader-table abstraction or `DispatchRays` API.
- **Mesh and amplification shaders:** no dedicated pipeline descriptors or dispatch API are exposed.
- **Traditional resource binding:** there is no legacy per-draw descriptor-table binding path. The public resource-binding model is bindless.
- **Portable native windows:** the public swapchain accepts Win32 `HWND` only.
