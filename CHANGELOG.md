# Changelog

This file lists the main user-visible changes in each Ldx12 version.

## 0.2.0 - Unreleased

### Added

- GPU capability validation during initialization, including feature level, Shader Model 6.6 and Resource Binding Tier 3.
- `DeviceProperties` for querying the selected adapter, memory and supported D3D12 capabilities.
- A bindless sampler heap with four built-in samplers and four slots for samplers created at runtime.
- `SamplerHandle`, `SamplerDesc` and the API required to create, query and destroy custom samplers.
- A native Ldx12 renderer for Dear ImGui that accepts `TextureHandle` directly in `ImGui::Image()`.
- An optional `Ldx12::Utils` world renderer with lazy cube and sphere batching, SRV-backed transforms and multi-draw indirect rendering.
- New Cookbook cube, texture-sampler, Ldx12 ImGui and native D3D12 ImGui samples.
- A full-capacity test covering the documented resource, attachment, command-buffer and push-constant limits.
- Optional PIX GPU capture loading and HUD settings through `PixSettings`.

### Changed

- Command buffers now reuse a fixed internal pool instead of allocating their implementation dynamically.
- Descriptor heaps and graphics/compute root signatures are bound once when command recording begins.
- The CBV + SRV cubes sample was simplified to make its resource layout and rendering flow easier to follow.
- Core implementation variables now use explicit types where type inference made the code harder to read.
- Context initialization and the App ImGui integration were simplified.
- Sampler selection is now bindless and available to graphics, compute, mip generation and ImGui shaders.

### Fixed

- GitHub Actions now builds and runs the full-capacity test with the rest of the core test suite.
- Changed-file detection now recognizes modifications to the full-capacity test.

## 0.1.0

- Initial packaged release of Ldx12.
