# Ldx12 tests

Ldx12 includes regular tests for its public API, three stress tests for its main resource and submission paths, and a full-capacity test.

## Capacity test

`Ldx12FullCapacityTests` reaches the documented limits for live buffers, textures and swapchains. It also validates backbuffer counts, eight color attachments, 16 vertex inputs, 64 acquired command buffers, four-buffer batches, 256 tracked textures and 63 push-constant values.

```bat
cmake --build build --config Debug --target Ldx12FullCapacityTests --parallel
ctest --test-dir build -C Debug -R Ldx12FullCapacityTests --output-on-failure --verbose
```

## Stress tests

- `Ldx12StressTests` exercises resource creation, bindless descriptors, command batches and texture-state transitions.
- `Ldx12FrameStressTests` processes 10,000 offscreen frames using three reusable frame slots. It waits only when the GPU is still using a slot.
- `Ldx12CommandStreamStressTests` sends 10,000 command buffers while previous submissions are pending. It verifies automatic command-buffer and allocator recycling.

Build and run an individual stress test from the repository root:

```bat
cmake --build build --config Debug --target Ldx12StressTests --parallel
ctest --test-dir build -C Debug -R Ldx12StressTests --output-on-failure --verbose

cmake --build build --config Debug --target Ldx12FrameStressTests --parallel
ctest --test-dir build -C Debug -R Ldx12FrameStressTests --output-on-failure --verbose

cmake --build build --config Debug --target Ldx12CommandStreamStressTests --parallel
ctest --test-dir build -C Debug -R Ldx12CommandStreamStressTests --output-on-failure --verbose
```
