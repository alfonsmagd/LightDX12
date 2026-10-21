# Triplanar Mapping

Direct3D 12 port of LightweightVK's `009_TriplanarMapping` sample. It renders 16 rotating icosahedra and applies two textures without stored UV coordinates.

Unlike the source sample, this version deliberately uses the conventional input assembler:

- `BufferType::Vertex` for interleaved positions and colors.
- `BufferType::Index` with `DXGI_FORMAT_R16_UINT` for the icosahedron indices.
- `CmdDrawIndexed(..., 16)` for instanced rendering.

The pixel shader projects each texture on the XY, ZY and XZ planes. It raises the absolute object-space normal to the eighth power, normalizes those three weights, and blends the projected samples. The texture and per-object matrix resources remain bindless.

The first texture reproduces the procedural 256 x 256 XOR pattern. The second loads `media/bistro/BuildingTextures/wood_polished_01_diff.png` when that optional Bistro asset is available; otherwise the sample creates a procedural wood texture so it remains self-contained.

Model matrices use three frame slots, each with its own buffer and submission fence, so CPU updates do not overwrite matrices still in use by the GPU.

The geometry, animation and projection technique are adapted from [LightweightVK](https://github.com/corporateshark/lightweightvk/blob/master/samples/009_TriplanarMapping.cpp), copyright (c) 2023-2026 Sergey Kosarevsky and contributors, under the [MIT license](LICENSE.md).
