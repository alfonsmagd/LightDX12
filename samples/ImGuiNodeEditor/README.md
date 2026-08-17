# ImGui Node Editor

This sample implements a small typed node graph and editor without changing
`LightD3D12` and without depending on a separate node-editor library.

## Included node types

- Float constant.
- Add, subtract and multiply.
- `Vector3` composition with three float inputs: X, Y and Z.
- Vector length, demonstrating a `Vector3` input and float output.
- A `Texture2D` source containing a procedural checker/gradient texture.
- A minimal CPU modifier that inverts every RGB pixel when its button is
  pressed, then uploads the modified pixel array again.
- A texture-preview node with a typed `Texture2D` input and an embedded image.

`App::NodeGraph` owns nodes, typed pins, links, cycle validation and recursive
evaluation. The sample owns only visual state such as node positions, canvas
panning and mouse interaction.

## Controls

- Drag a node header to move it.
- Click or use the +/- controls on a constant to edit it during runtime; all
  downstream node results update immediately.
- Drag from one pin to a compatible pin to connect them.
- Right-click a connected input pin to disconnect it.
- Right-click empty canvas space to create a node.
- Hold the middle mouse button to pan the canvas.
- Select a node and press Delete to remove it.

The initial graph calculates `(3 + 2, 3 - 2, (3 + 2) * (3 - 2))`, producing
`Vector3(5, 1, 5)`, and then calculates its length. A second branch connects a
real LightD3D12 texture through `Invertir textura CPU` to the preview node. The
modifier loops over the RGBA8 array in RAM; no compute or pixel shader modifies
the image. The App layer registers the uploaded texture in ImGui's descriptor
heap; the renderer itself remains unchanged.
