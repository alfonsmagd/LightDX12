# CBV + SRV Cubes

This sample gives each bindless buffer one clear responsibility:

- The CBV contains `SceneConstants`, shared by every cube: aspect ratio, view distance and light direction.
- The SRV contains `CubeData[]`, with one model matrix and color per cube.

`CmdDraw( 36, cubeCount )` draws every cube in one instanced call. The vertex shader uses `SV_InstanceID` to read `CubeData[instanceID]` directly from the SRV.
