ldx12_add_example(19_DrawIndirectGltf DrawIndirectGltf
    SOURCES DrawIndirectModel.hpp shaders/DrawIndirectGltf.hlsl shaders/DrawIndirectSkybox.hlsl
    ROOT_ASSETS
        media/sky_129_cubemap_2k/px.png
        media/sky_129_cubemap_2k/nx.png
        media/sky_129_cubemap_2k/py.png
        media/sky_129_cubemap_2k/ny.png
        media/sky_129_cubemap_2k/pz.png
        media/sky_129_cubemap_2k/nz.png
    LIBRARIES Ldx12::Ldx12 Ldx12::Utils GltfSampleCommon Ldx12::ImGui comdlg32
    FOLDER ThirdParty LABEL "19. GltfScene (MultiDrawIndirect)")
