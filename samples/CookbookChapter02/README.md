# Cookbook Chapter 02 cube

Ldx12 adaptation of the [`Chapter02/03_GLM`](https://github.com/PacktPublishing/3D-Graphics-Rendering-Cookbook-Second-Edition/tree/main/Chapter02/03_GLM) example from *3D Graphics Rendering Cookbook, Second Edition*.

It renders the same rotating colored cube twice: first as solid geometry and then as a black wireframe overlay. The 36 cube vertices are selected inside the vertex shader and the CPU sends only the current MVP matrix through push constants.

This version uses the native Win32 window path and DirectXMath, so it does not depend on GLFW or GLM.
