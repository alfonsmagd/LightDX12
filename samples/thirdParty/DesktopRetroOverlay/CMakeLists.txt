add_library(DesktopRetroCapture STATIC DesktopCapture.cpp DesktopCapture.hpp Win32Support.hpp)
target_link_libraries(DesktopRetroCapture PUBLIC Ldx12::Ldx12 PRIVATE d3d11 windowsapp)
target_compile_features(DesktopRetroCapture PUBLIC cxx_std_20)
add_library(DesktopRetroEffects STATIC RetroRenderer.cpp RetroRenderer.hpp RetroSettings.hpp EffectPresets.hpp)
target_link_libraries(DesktopRetroEffects PUBLIC Ldx12::Ldx12)
target_include_directories(DesktopRetroEffects PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
add_executable(DesktopRetroOverlay WIN32
    main.cpp
    OverlayApp.cpp OverlayApp.hpp
    OverlayApplication.cpp OverlayApplication.hpp
    AppOptions.cpp AppOptions.hpp
    Diagnostics.cpp Diagnostics.hpp
    OverlayControls.cpp OverlayControls.hpp)
target_link_libraries(DesktopRetroOverlay PRIVATE DesktopRetroCapture DesktopRetroEffects shell32 windowsapp)
set(RETRO_SHADERS shaders/DesktopRetroOverlayVS.hlsl shaders/DesktopRetroOverlayPS.hlsl
    shaders/NewPixieAccumulatePS.hlsl shaders/NewPixieBlurPS.hlsl)
set_source_files_properties(${RETRO_SHADERS} PROPERTIES HEADER_FILE_ONLY TRUE)
target_sources(DesktopRetroOverlay PRIVATE ${RETRO_SHADERS})
set_target_properties(DesktopRetroCapture DesktopRetroEffects DesktopRetroOverlay PROPERTIES FOLDER "thirdParty/DesktopRetroOverlay")
if(MSVC)
    target_compile_options(DesktopRetroCapture PRIVATE /W4 /EHsc /permissive- /MP)
    target_compile_options(DesktopRetroEffects PRIVATE /W4 /EHsc /permissive- /MP)
    target_compile_options(DesktopRetroOverlay PRIVATE /W4 /EHsc /permissive- /MP)
endif()

# Always copy shaders when this target builds, including shader-only edits.
add_custom_target(DesktopRetroShaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders" "$<TARGET_FILE_DIR:DesktopRetroOverlay>/DesktopRetroOverlay/shaders"
    VERBATIM)
add_dependencies(DesktopRetroOverlay DesktopRetroShaders)
set_target_properties(DesktopRetroShaders PROPERTIES FOLDER "thirdParty/DesktopRetroOverlay")

if(LDX12_BUILD_TESTS)
    add_executable(DesktopRetroEffectTests tests/EffectTests.cpp)
    target_link_libraries(DesktopRetroEffectTests PRIVATE DesktopRetroEffects)
    target_compile_definitions(DesktopRetroEffectTests PRIVATE RETRO_TEST_SHADER_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME DesktopRetroEffectTests COMMAND DesktopRetroEffectTests)
    set_tests_properties(DesktopRetroEffectTests PROPERTIES TIMEOUT 60)
    set_target_properties(DesktopRetroEffectTests PROPERTIES FOLDER "Folder_Test")
endif()
