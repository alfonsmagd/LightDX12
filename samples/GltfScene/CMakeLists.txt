ldx12_add_example(18_GltfScene GltfScene
    SOURCES SceneModel.hpp shaders/GltfScene.hlsl
    LIBRARIES Ldx12::Ldx12 Ldx12::Utils GltfSampleCommon Ldx12::ImGui comdlg32
    FOLDER ThirdParty LABEL "18. GltfScene (Direct Draws)")
