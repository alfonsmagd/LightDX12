# CB + SRV Cubes

Small sample for comparing two bindless buffer paths:

- `MatrixRows` records are uploaded to a `StructuredBuffer` exposed as an SRV.
- `CubeColorConstants` is uploaded to a constant buffer exposed as a CBV.
- The shader lives in `shaders/CBSRVCubes.hlsl`, includes `LightD3D12_Defines.hlsli`, and reads both resources from fixed slots in `ResourceDescriptorHeap`.

This is intentionally simple: cube geometry is generated in the vertex shader with `SV_VertexID`, and instances are selected with `SV_InstanceID`.
