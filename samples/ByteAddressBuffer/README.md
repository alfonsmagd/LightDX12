# ByteAddressBuffer

A rotating cube with the shared `media/ldx12-cube.png` logo and one tint per face, uploaded through Ldx12 as a raw SRV.

- C++ stores six RGBA colors as consecutive `std::array<float, 4>` values.
- `BufferDesc::type = BufferType::Raw` creates a raw buffer view, bound to `ShaderResourceSlot::FreeSRV0`. Leave `stride` at zero.
- HLSL accesses the same slot as a `ByteAddressBuffer` through `ResourceDescriptorHeap`.
- The CPU assigns each vertex a color byte offset using `(vertexIndex / 6) * 16`. HLSL uses `Load4(input.colorByteOffset)` to read four 32-bit words, and `asfloat` reinterprets those bits as RGBA floats.
- The CPU builds model, view and perspective matrices (near = 1, far = 20, view distance = 6). A separate CBV holds the combined matrix, model matrix, normalized light direction and light color, updated each frame.
- The CPU interpolates the light from white to blue in three seconds, then back to white in three seconds. The pixel shader applies the uploaded RGB value to the lighting.

The raw buffer contains 96 bytes. Byte offsets must be multiples of four; each color starts at a multiple of 16. Geometry and clockwise face ordering are defined on the CPU and uploaded through a vertex buffer, then rendered with `CmdDraw(36)`. The vertex shader only applies the uploaded matrices and reads the raw color buffer.

Build target: `22_ByteAddressBuffer`. Executable: `ByteAddressBuffer.exe`.

The CPU supplies UVs alongside the color byte offset. The pixel shader samples the shared logo and applies `texture * (0.5 + 0.5 * faceColor)` before lighting.
