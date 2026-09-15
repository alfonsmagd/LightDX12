# Third-party notices

Ldx12 includes or builds with the following third-party software. Each component remains under its own license.

## Microsoft D3DX12 utility header

`Ldx12/src/d3dx12.h` is sourced from Microsoft's DirectX Graphics Samples.

Copyright (c) Microsoft. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

## Dear ImGui

The optional App and ImGui examples use Dear ImGui, copyright (c) 2014-2026 Omar Cornut, under the MIT License. Its complete license is preserved in `third_party/imgui/LICENSE.txt`.

## 3D Graphics Rendering Cookbook, Second Edition

`samples/CookbookChapter02` adapts the `Chapter02/03_GLM` example from [3D Graphics Rendering Cookbook, Second Edition](https://github.com/PacktPublishing/3D-Graphics-Rendering-Cookbook-Second-Edition).

`samples/GltfScene` also adapts the metallic/roughness environment-lighting approach from `Chapter06/04_MetallicRoughness`. Its precomputed KTX environment files are stored in `media/pbr`; the original Piazza Bologni HDRI is by Andreas Mischok / Poly Haven under CC0. See `media/pbr/README.md` for asset provenance.

Copyright (c) 2024 Packt.

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

## cgltf

The optional Ldx12Utils glTF loader uses cgltf from https://github.com/jkuhlmann/cgltf at commit `85cd62382dfea638278962690cf515023f33ed00`. The original license is also preserved in `third_party/cgltf/LICENSE`.

Copyright (c) 2018-2021 Johannes Kuhlmann

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

## JSMN

cgltf embeds the JSMN JSON parser from https://github.com/zserge/jsmn under the MIT License.

Copyright (c) 2010 Serge A. Zaitsev

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

## Damaged Helmet sample asset

The supplied model in media/gltfMesh has separate attribution and Creative Commons license terms documented in media/gltfMesh/README.md. It is not included in the NuGet package.

## Desktop Retro Overlay

The optional `samples/thirdParty/DesktopRetroOverlay` integration uses Windows Graphics
Capture through the Windows SDK/D3D11, with Ldx12/D3D12 rendering. Shader adaptation
credits and license notices for Simple CRT Shader (yunoda, MIT) and NewPixie CRT
(Mattias Gustavsson, public-domain alternative) are preserved in
[`samples/thirdParty/DesktopRetroOverlay/THIRD_PARTY_NOTICES.md`](samples/thirdParty/DesktopRetroOverlay/THIRD_PARTY_NOTICES.md).
