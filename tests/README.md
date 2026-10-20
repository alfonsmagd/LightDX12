# Ldx12 tests

The test suite covers the public API, GPU resource lifetime, bindless descriptors, command submission, fixed capacities and optional utilities.

## Run the suite

Generate `build/Ldx12.sln` with `GenerateSolution.bat`, or configure the repository with CMake. Then build and run all enabled tests:

```bat
cmake --build build --config Debug --parallel
ctest --test-dir build -C Debug --output-on-failure
```

Use `Release` in both commands to test the release configuration.

## Core API and GPU tests

`Ldx12Tests` contains the main CPU and GPU contract checks:

- Generational `SlotMap` creation, destruction, reuse, stale handles, stable storage, fragmented-slot reuse, fixed capacity and virtual objects.
- Adapter selection through the public initialization API, selected device properties and fallback after a nonexistent index.
- Buffer, texture and sampler lifetime, including invalid-handle behavior.
- Deferred sampler destruction and descriptor reuse after GPU completion.
- Bindless SRV/UAV descriptor allocation, fragmentation, coalescing and recycling.
- Structured and raw UAV compute, explicit state transitions, UAV barriers and CPU readback.
- Command-buffer batches, synchronization, recycling and `Discard()` recovery.
- Multisample rendering and resolve.
- Shader access to `Texture2DArray` and `TextureCube` views.

The compute test also exercises the typed `CmdPushConstants(constants)` overload. Build and run this target with:

```bat
cmake --build build --config Debug --target Ldx12Tests --parallel
ctest --test-dir build -C Debug -R "^Ldx12Tests$" --output-on-failure
```

For a shorter feedback loop, list or select cases directly. `CPU` and `GPU` select their group; other filters match a case-sensitive substring of the displayed name. An unmatched filter fails instead of silently passing:

```bat
build\tests\Ldx12\Debug\Ldx12Tests.exe --list
build\tests\Ldx12\Debug\Ldx12Tests.exe CPU
build\tests\Ldx12\Debug\Ldx12Tests.exe "SlotMap capacity"
build\tests\Ldx12\Debug\Ldx12Tests.exe "GPU descriptor"
```

`CPU` runs without initializing a graphics device. With no argument, the executable runs all cases and reports every failure. CTest keeps the existing `Ldx12Tests` entry and runs the complete suite once.

### Core scenarios

The core executable has eleven cases: four CPU cases, adapter selection and six GPU resource-use scenarios. Capacity and sustained pressure have their own executables.

| Source | Scenario |
| --- | --- |
| `Ldx12/SlotMapCreationTest.cpp` | Create, retrieve and iterate objects; destroy/reuse a slot; reject stale handles; clear and refill |
| `Ldx12/SlotMapCapacityTest.cpp` | Fill a four-slot map, reject overflow, copy and move fragmented storage |
| `Ldx12/SlotMapFragmentationTest.cpp` | Preserve the 32/53-hole regression across three 64-slot regions |
| `Ldx12/SlotMapVirtualObjectTest.cpp` | Release owned polymorphic objects on destruction and clear |
| `Ldx12/AdapterSelectionTest.cpp` | Initialize the default GPU, inspect its properties and verify fallback from a nonexistent index |
| `Ldx12/GpuResourceLifecycleTest.cpp` | Upload/update a buffer, upload/read back texture pixels, destroy/recreate resources and reject retired handles |
| `Ldx12/GpuComputeTest.cpp` | Write a structured UAV, consume it as an SRV, write a raw UAV and verify CPU readback |
| `Ldx12/GpuSubmissionTest.cpp` | Discard a resource transition, record two render passes before submission and verify the final texture pixels |
| `Ldx12/GpuDescriptorRecyclingTest.cpp` | Reuse descriptor holes and ranges across textures/buffers, and reclaim RTV/DSV/fixed slots |
| `Ldx12/GpuMultisampleTest.cpp` | Draw a white triangle over red, resolve MSAA and verify the interior, background and partially covered edges |
| `Ldx12/GpuTextureViewTest.cpp` | Sample an array slice and cube face, retire the sampler during the pending draw and reuse its descriptor after completion |

GPU checks prefer observable results over inspecting native resource descriptions. Test expectations use exceptions so they remain active in Release. Constant-buffer usage is covered by the dedicated compute and rendering regressions below.

## Focused infrastructure tests

| Target | Coverage |
| --- | --- |
| `Ldx12NativeImportTests` | Native texture import, COM ownership, pixels, invalid inputs and descriptor reuse |
| `Ldx12DeferredTests` | CPU cleanup-guard semantics plus GPU ring retirement, interleaved reservations and full-capacity reuse |
| `Ldx12FullCapacityTests` | Live-resource, swapchain, attachment, vertex-input, command-buffer, state-tracking and push-constant limits |

The capacity test uses the compiled resource profile, checks oversized heap rejection, and reaches 64 acquired command buffers, batches of four, the profile tracking limit and 62 push-constant values. Configure `LDX12_RESOURCE_PROFILE=SMALL`, `MEDIUM` or `HIGH` to exercise each profile:

```bat
cmake --build build --config Debug --target Ldx12FullCapacityTests --parallel
ctest --test-dir build -C Debug -R "^Ldx12FullCapacityTests$" --output-on-failure
```

## Stress tests

- `Ldx12StressTests` repeatedly creates resources, allocates bindless descriptors, submits command batches and transitions texture states.
- `Ldx12FrameStressTests` processes 10,000 offscreen frames with three reusable frame slots and waits only when a slot is still in use.
- `Ldx12CommandStreamStressTests` submits 10,000 command buffers while earlier submissions remain pending, verifying command-buffer and allocator recycling.

Run any target by name, for example:

```bat
cmake --build build --config Debug --target Ldx12CommandStreamStressTests --parallel
ctest --test-dir build -C Debug -R "^Ldx12CommandStreamStressTests$" --output-on-failure
```

## Optional tests

These targets are enabled by their corresponding build components:

| Target | Coverage |
| --- | --- |
| `Ldx12UtilsTests` | Geometry helpers, `World`, `DebugRenderer` and offscreen rendering |
| `Ldx12GltfTests` | glTF, GLB, external/base64 buffers, transforms, normals, colors and invalid inputs |
| `Ldx12GltfMaterialTests` | DamagedHelmet material uploads, mip chains, sRGB selection, samplers, HDR environment and PBR shader compilation |
| `Ldx12NodeGraphTests` | App node connections, evaluation, type validation and texture references |
| `DesktopRetroEffectTests` | Headless retro presets, resize behavior, temporal-history reset and D3D12 debug-layer errors |

The glTF tests require Utils and third-party support. `Ldx12NodeGraphTests` requires the App target. `DesktopRetroEffectTests` is enabled with the desktop retro overlay.

## Constant-buffer ring regression tests

- `Ldx12ConstantBufferTests` reads CB0 and CB1 through the real HLSL header, verifies per-dispatch offsets and offset preservation, checks recording reset, and gates GPU submission to test deferred retirement during command-buffer reuse.
- `Ldx12ConstantBufferRenderTests` renders the ConstantBufferRings sample offscreen for 900 frames at 320 x 180. It wraps ring 0, verifies the red cube and constant light color by pixel readback, checks animation changes, and rejects D3D12 debug-layer errors. Requires `LDX12_BUILD_UTILS=ON` or examples enabled.
- `Ldx12DeferredTests` exercises ring lifetime through public batch submission, including interleaved reservations and full-capacity reuse.

```bat
cmake --build build --config Debug --target Ldx12ConstantBufferTests Ldx12ConstantBufferRenderTests Ldx12DeferredTests
ctest --test-dir build -C Debug -R "^Ldx12(ConstantBuffer|ConstantBufferRender|Deferred)Tests$" --output-on-failure
```

The compute and deferred tests run in core CI; the 900-frame rendering test runs in Utils CI. Use `Release` in both commands to check the release configuration.
