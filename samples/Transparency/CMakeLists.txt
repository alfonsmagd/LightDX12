ldx12_add_example(12_Transparency Transparency
    SOURCES shaders/TransparencyCommon.hlsli shaders/TransparencyQuad.hlsl shaders/TransparencySkybox.hlsl
    ROOT_ASSETS
        media/sky_129_cubemap_2k/cubemap_layout.png
        media/sky_129_cubemap_2k/px.png
        media/sky_129_cubemap_2k/nx.png
        media/sky_129_cubemap_2k/py.png
        media/sky_129_cubemap_2k/ny.png
        media/sky_129_cubemap_2k/pz.png
        media/sky_129_cubemap_2k/nz.png
    LIBRARIES Ldx12::Ldx12 Ldx12::Utils)
