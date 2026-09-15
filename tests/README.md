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

- Generational `SlotMap` creation, destruction, reuse, stale handles, fixed capacity and virtual objects.
- Public fixed-array properties and documented capacities.
- Buffer, texture and sampler lifetime, including invalid-handle behavior.
- Deferred sampler destruction and descriptor reuse after GPU completion.
- Bindless SRV/UAV descriptor allocation, fragmentation, coalescing and recycling.
- Structured and raw UAV compute, explicit state transitions, UAV barriers and CPU readback.
- Command-buffer batches, synchronization, recycling and `Discard()` recovery.
- Multisample rendering and resolve.
- Shader access to `Texture2DArray` and `TextureCube` views.
- Texture-format queries and framebuffer/pipeline color-format warnings through the D3D12 info queue.

The compute test also exercises the typed `CmdPushConstants(constants)` overload. Build and run this target with:

```bat
cmake --build build --config Debug --target Ldx12Tests --parallel
ctest --test-dir build -C Debug -R "^Ldx12Tests$" --output-on-failure
```

## Focused infrastructure tests

| Target | Coverage |
| --- | --- |
| `Ldx12NativeImportTests` | Native texture import, COM ownership, pixels, invalid inputs and descriptor reuse |
| `Ldx12DeferredTests` | `DeferredRelease::OnFailure` on normal exit, exception unwinding and locally handled exceptions; no GPU required |
| `Ldx12FullCapacityTests` | Live-resource, swapchain, attachment, vertex-input, command-buffer, state-tracking and push-constant limits |

The capacity test reaches 64 acquired command buffers, batches of four, 256 tracked textures and 63 push-constant values:

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
