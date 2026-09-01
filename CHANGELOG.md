# Changelog

This file lists the main user-visible changes in each Ldx12 version.

## 0.3.0 - Unreleased

### Added

- A compute wind-wake sample with an orbital camera, runtime cube/sphere insertion and a 512 x 512 vector field with vortices. It detects only downwind obstacle-edge seeds, propagates them through the local wind field and stores the strongest attenuation in a race-safe integer wake texture.

### Fixed

- Replaced the `ICommandBuffer`/`CommandBufferImpl` split with one concrete `CommandBuffer`, removing virtual command dispatch and interface-to-implementation conversion during submission.

## 0.2.0 - 2026-08-30

### Added

- GPU capability validation during initialization, including feature level, Shader Model 6.6 and Resource Binding Tier 3.
- `DeviceProperties` for querying the selected adapter, memory and supported D3D12 capabilities.
- A bindless sampler heap with four built-in samplers and four slots for samplers created at runtime.
- `SamplerHandle`, `SamplerDesc` and the API required to create, query and destroy custom samplers.
- A native Ldx12 renderer for Dear ImGui that accepts `TextureHandle` directly in `ImGui::Image()`.
- An optional `Ldx12::Utils` `DebugRenderer` with predefined cube, sphere and arrow geometry, per-instance colors and multi-draw indirect batching.
- Reusable `Ldx12::Utils` geometry, orbit-camera, image/cubemap-loading and resize-aware depth-target helpers.
- Configurable multisample rendering through `TextureDesc::sampleCount`, `RenderPipelineDesc::sampleCount`, capability queries and `CmdResolveTexture()`.
- New textured-cube, Cookbook cube, texture-sampler, Z-fighting, Ldx12 ImGui and native D3D12 ImGui samples.
- A full-capacity test covering the documented resource, attachment, command-buffer and push-constant limits.
- A GPU multisample test covering MSAA color/depth creation, rendering, resolve and texture readback.
- Optional PIX GPU capture loading and HUD settings through `PixSettings`.
- Native sampled `TextureCube` support through `TextureDimension::TextureCube`: six packed faces are uploaded into one resource and exposed as a bindless `TextureCube` SRV.
- Sampled `Texture2DArray` resources with packed slice uploads and bindless SRVs.
- A complete cubemap sample with six image faces, a reflective cube or sphere, an orbit camera, a skybox and separate HLSL shader files.
- Reusable `CreateQuad()` geometry in `Ldx12Utils`.
- A `Texture2DArray` sample with four independently moving triangles, each sampling a different array slice.
- A transparency sample that separates opaque depth-writing from alpha-blended rendering over a cubemap skybox.
- A depth-prepass sample that renders three moving cubes at different distances into the backbuffer and displays the sampled depth buffer through Dear ImGui.

### Changed

- Buffer creation now uses a compact typed `BufferDesc` with simple `BufferType` and `BufferMemory` enums instead of exposing raw D3D12 heap, flag and view options.
- Buffer creation was reorganized into validation, resource creation and descriptor creation steps, with automatic resource states and initial-data handling.
- Command buffers now reuse a fixed internal pool instead of allocating their implementation dynamically.
- Descriptor heaps and graphics/compute root signatures are bound once when command recording begins.
- The CBV + SRV cubes sample was simplified to make its resource layout and rendering flow easier to follow.
- Core implementation variables now use explicit types where type inference made the code harder to read.
- Context initialization and the App ImGui integration were simplified.
- Sampler selection is now bindless and available to graphics, compute, mip generation and ImGui shaders.
- Utils geometry is uploaded once; object changes rebuild only instance and indirect buffers, while transform changes update only instances.
- Utils arrows now render as continuous line geometry with an outline arrowhead.
- The Z-fighting sample can switch between single-sample rendering and MSAA x4 at runtime through a fixed ImGui control panel.
- Repeated Win32 window, resize, input and message-loop boilerplate is shared by all Ldx12 samples through `ldx12::utils::AppLdx`; the intentionally native D3D12 sample remains standalone.
- The cubemap sample now receives ready-to-draw GPU buffer handles from `CreateCube()` and `CreateSphere()` and keeps the Ldx12 rendering commands explicit.
- TexturedCube reuses the cube buffers from `Ldx12Utils`, while texture-focused samples reuse the common checker-texture generator.
- Sample projects are numbered consistently from 01 to 15 in Visual Studio and in the documentation.
- Sampled depth SRVs replicate their depth value across RGB, so they can be inspected directly with `ImGui::Image()`.

### Fixed

- GitHub Actions now builds and runs the full-capacity test with the rest of the core test suite.
- Changed-file detection now recognizes modifications to the full-capacity test.

## 0.1.0

- Initial packaged release of Ldx12.
