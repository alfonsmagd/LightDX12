ldx12_add_example(19_DrawIndirectGltf DrawIndirectGltf
    SOURCES DrawIndirectModel.hpp shaders/DrawIndirectGltf.hlsl
    LIBRARIES Ldx12::Ldx12 Ldx12::Utils GltfSampleCommon Ldx12::ImGui comdlg32
    FOLDER ThirdParty LABEL "19. GltfScene (MultiDrawIndirect)")
