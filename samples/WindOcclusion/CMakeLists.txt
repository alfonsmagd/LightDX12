ldx12_add_example(16_WindOcclusion WindOcclusion
    SOURCES shaders/WindCommon.hlsli shaders/ObstacleMask.hlsl shaders/WakeGenerate.hlsl
            shaders/WakeAverage.hlsl shaders/WakeClear.hlsl shaders/WindField.hlsl
            shaders/WakePreview.hlsl shaders/SceneObstacle.hlsl
    LIBRARIES Ldx12::Ldx12 Ldx12::Utils Ldx12::ImGui)
