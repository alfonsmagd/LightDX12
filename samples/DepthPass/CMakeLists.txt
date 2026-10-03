ldx12_add_example(15_DepthPass DepthPass
    SOURCES ../CubeTexture.hpp shaders/DepthPass.hlsl
    ROOT_ASSETS media/ldx12-cube.png
    LIBRARIES Ldx12::Ldx12 Ldx12::Utils Ldx12::ImGui)
