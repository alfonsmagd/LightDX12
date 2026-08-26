cmake_minimum_required(VERSION 3.24)

project(Ldx12 VERSION 0.1.0 LANGUAGES CXX)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
option(LDX12_BUILD_APP "Build the optional App support library" ${PROJECT_IS_TOP_LEVEL})
option(LDX12_BUILD_EXAMPLES "Build the Ldx12 examples" ${PROJECT_IS_TOP_LEVEL})
option(LDX12_BUILD_TESTS "Build the Ldx12 tests" ${PROJECT_IS_TOP_LEVEL})
option(LDX12_INSTALL "Generate installation rules for Ldx12" ${PROJECT_IS_TOP_LEVEL})
if(LDX12_BUILD_TESTS)
    enable_testing()
endif()

if(NOT WIN32)
    message(FATAL_ERROR "Ldx12 currently supports only Windows.")
endif()

include(GNUInstallDirs)

set(LDX12_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/Ldx12")
set(LDX12_BASE_MIPS_SHADER "${LDX12_ROOT}/src/shaders/Ldx12BaseMipsCS.hlsl")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${LDX12_BASE_MIPS_SHADER}")
file(READ "${LDX12_BASE_MIPS_SHADER}" LDX12_BASE_MIPS_SHADER_SOURCE)
set(LDX12_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${LDX12_GENERATED_DIR}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Ldx12BaseMipsShader.hpp.in"
    "${LDX12_GENERATED_DIR}/Ldx12BaseMipsShader.hpp"
    @ONLY
)

set(LDX12_PUBLIC_HEADERS
    "${LDX12_ROOT}/include/Ldx12/HandleSlotMap.hpp"
    "${LDX12_ROOT}/include/Ldx12/Ldx12.hpp"
    "${LDX12_ROOT}/include/Ldx12/Ldx12Native.hpp"
    "${LDX12_ROOT}/include/Ldx12/Ldx12_Defines.hpp"
    "${LDX12_ROOT}/include/Ldx12/HLSLLoader.hpp"
)

set(LDX12_PRIVATE_HEADERS
    "${LDX12_ROOT}/src/d3dx12.h"
    "${LDX12_ROOT}/src/Ldx12BaseMips.hpp"
    "${LDX12_ROOT}/src/Ldx12CommandBatch.hpp"
    "${LDX12_ROOT}/src/Ldx12CommandBuffer.hpp"
    "${LDX12_ROOT}/src/Ldx12ImmediateCommands.hpp"
    "${LDX12_ROOT}/src/Ldx12Internal.hpp"
    "${LDX12_ROOT}/src/Ldx12ShaderCompiler.hpp"
    "${LDX12_ROOT}/src/Ldx12StagingDevice.hpp"
    "${LDX12_ROOT}/src/Ldx12Swapchain.hpp"
)

set(LDX12_GENERATED_HEADERS
    "${LDX12_GENERATED_DIR}/Ldx12BaseMipsShader.hpp"
)

set(LDX12_SOURCES
    "${LDX12_ROOT}/src/Ldx12BaseMips.cpp"
    "${LDX12_ROOT}/src/Ldx12CommandBatch.cpp"
    "${LDX12_ROOT}/src/Ldx12CommandBuffer.cpp"
    "${LDX12_ROOT}/src/Ldx12DeviceManager.cpp"
    "${LDX12_ROOT}/src/Ldx12ImmediateCommands.cpp"
    "${LDX12_ROOT}/src/Ldx12Native.cpp"
    "${LDX12_ROOT}/src/Ldx12RenderDevice.cpp"
    "${LDX12_ROOT}/src/Ldx12Resources.cpp"
    "${LDX12_ROOT}/src/Ldx12ShaderCompiler.cpp"
    "${LDX12_ROOT}/src/Ldx12StagingDevice.cpp"
    "${LDX12_ROOT}/src/Ldx12Swapchain.cpp"
    "${LDX12_ROOT}/src/HLSLLoader.cpp"
)

set(LDX12_SHADERS
    "${LDX12_BASE_MIPS_SHADER}"
    "${LDX12_ROOT}/src/shaders/Ldx12_Defines.hlsli"
)

set_source_files_properties(${LDX12_SHADERS} PROPERTIES HEADER_FILE_ONLY TRUE)

add_library(Ldx12 STATIC
    ${LDX12_PUBLIC_HEADERS}
    ${LDX12_PRIVATE_HEADERS}
    ${LDX12_GENERATED_HEADERS}
    ${LDX12_SOURCES}
    ${LDX12_SHADERS}
)

add_library(Ldx12::Ldx12 ALIAS Ldx12)

set_target_properties(Ldx12 PROPERTIES DEBUG_POSTFIX "d")

target_compile_features(Ldx12 PUBLIC cxx_std_20)
target_compile_definitions(Ldx12
    PUBLIC
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        UNICODE
        _UNICODE
)
target_include_directories(Ldx12
    PUBLIC
        "$<BUILD_INTERFACE:${LDX12_ROOT}/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    PRIVATE
        "${LDX12_ROOT}/src"
        "${LDX12_GENERATED_DIR}"
)
target_link_libraries(Ldx12
    PUBLIC
        d3d12
        d3dcompiler
        dxgi
        dxguid
)

if(MSVC)
    target_compile_options(Ldx12 PRIVATE /W4 /EHsc /permissive- /MP)
endif()

if(LDX12_INSTALL)
    include(CMakePackageConfigHelpers)
    set(LDX12_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/Ldx12")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/cmake")
    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Ldx12Config.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake/Ldx12Config.cmake"
        INSTALL_DESTINATION "${LDX12_INSTALL_CMAKEDIR}"
    )
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/cmake/Ldx12ConfigVersion.cmake"
        VERSION "${PROJECT_VERSION}"
        COMPATIBILITY SameMajorVersion
    )
    install(
        TARGETS Ldx12
        EXPORT Ldx12Targets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    )
    install(DIRECTORY "${LDX12_ROOT}/include/" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
    install(
        FILES
            "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
            "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/Ldx12"
    )
    install(
        EXPORT Ldx12Targets
        FILE Ldx12Targets.cmake
        NAMESPACE Ldx12::
        DESTINATION "${LDX12_INSTALL_CMAKEDIR}"
    )
    install(
        FILES
            "${CMAKE_CURRENT_BINARY_DIR}/cmake/Ldx12Config.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/cmake/Ldx12ConfigVersion.cmake"
        DESTINATION "${LDX12_INSTALL_CMAKEDIR}"
    )
endif()

set(APP_PUBLIC_HEADERS)
set(APP_SOURCES)
if(LDX12_BUILD_APP OR LDX12_BUILD_EXAMPLES)
    set(IMGUI_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui")
    add_library(DearImGui STATIC
        "${IMGUI_ROOT}/imconfig.h"
        "${IMGUI_ROOT}/imgui.h"
        "${IMGUI_ROOT}/imgui_internal.h"
        "${IMGUI_ROOT}/imgui.cpp"
        "${IMGUI_ROOT}/imgui_draw.cpp"
        "${IMGUI_ROOT}/imgui_tables.cpp"
        "${IMGUI_ROOT}/imgui_widgets.cpp"
        "${IMGUI_ROOT}/imstb_rectpack.h"
        "${IMGUI_ROOT}/imstb_textedit.h"
        "${IMGUI_ROOT}/imstb_truetype.h"
        "${IMGUI_ROOT}/backends/imgui_impl_win32.h"
        "${IMGUI_ROOT}/backends/imgui_impl_win32.cpp"
        "${IMGUI_ROOT}/backends/imgui_impl_dx12.h"
        "${IMGUI_ROOT}/backends/imgui_impl_dx12.cpp"
    )
    target_compile_features(DearImGui PUBLIC cxx_std_20)
    target_include_directories(DearImGui PUBLIC "${IMGUI_ROOT}")
    target_compile_definitions(DearImGui PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    target_link_libraries(DearImGui PUBLIC d3d12 dxgi)

    add_library(Ldx12ImGui STATIC
        "${CMAKE_CURRENT_SOURCE_DIR}/App/include/App/imgui_impl_ldx12.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/App/src/imgui_impl_ldx12.cpp"
    )
    add_library(Ldx12::ImGui ALIAS Ldx12ImGui)
    target_compile_features(Ldx12ImGui PUBLIC cxx_std_20)
    target_include_directories(Ldx12ImGui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/App/include")
    target_compile_definitions(Ldx12ImGui PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    target_link_libraries(Ldx12ImGui PUBLIC Ldx12::Ldx12 DearImGui)
    if(MSVC)
        target_compile_options(Ldx12ImGui PRIVATE /W4 /EHsc /permissive- /MP)
    endif()

endif()

if(LDX12_BUILD_APP)

    set(APP_PUBLIC_HEADERS
        "${CMAKE_CURRENT_SOURCE_DIR}/App/include/App/ImGuiLayer.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/App/include/App/NodeGraph.hpp"
    )
    set(APP_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/App/src/ImGuiLayer.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/App/src/NodeGraph.cpp"
    )
    add_library(App STATIC ${APP_PUBLIC_HEADERS} ${APP_SOURCES})
    add_library(Ldx12::App ALIAS App)
    target_compile_features(App PUBLIC cxx_std_20)
    target_include_directories(App PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/App/include")
    target_compile_definitions(App PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    target_link_libraries(App PUBLIC Ldx12::ImGui)
    if(MSVC)
        target_compile_options(App PRIVATE /W4 /EHsc /permissive- /MP)
    endif()
endif()

set(LDX12_EXAMPLE_SOURCES)
if(LDX12_BUILD_EXAMPLES)
    function(add_ldx12_sample target_name)
        add_executable(${target_name} WIN32 ${ARGN})
        target_link_libraries(${target_name} PRIVATE Ldx12::Ldx12)
        target_compile_features(${target_name} PRIVATE cxx_std_20)
        if(MSVC)
            target_compile_options(${target_name} PRIVATE /W4 /EHsc /permissive- /MP)
        endif()
    endfunction()

    set(TRIANGLE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/samples/Triangle/main.cpp")
    set(VIEWPORT_SCISSOR_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/samples/ViewportScissor/main.cpp")
    set(TEXTURE_SAMPLERS_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/samples/TextureSamplers/main.cpp")
    set(CBSRVCUBES_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/samples/CBSRVCubes/main.cpp")
    set(CBSRVCUBES_README "${CMAKE_CURRENT_SOURCE_DIR}/samples/CBSRVCubes/README.md")
    set(CBSRVCUBES_SHADER "${CMAKE_CURRENT_SOURCE_DIR}/samples/CBSRVCubes/shaders/CBSRVCubes.hlsl")
    set(COOKBOOK_CHAPTER02_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/samples/CookbookChapter02/main.cpp")
    set(COOKBOOK_CHAPTER02_README "${CMAKE_CURRENT_SOURCE_DIR}/samples/CookbookChapter02/README.md")
    set(COOKBOOK_CHAPTER02_SHADER "${CMAKE_CURRENT_SOURCE_DIR}/samples/CookbookChapter02/shaders/CookbookCube.hlsl")
    set_source_files_properties("${CBSRVCUBES_SHADER}" PROPERTIES HEADER_FILE_ONLY TRUE)
    set_source_files_properties("${COOKBOOK_CHAPTER02_SHADER}" PROPERTIES HEADER_FILE_ONLY TRUE)
    add_ldx12_sample(Triangle "${TRIANGLE_SOURCE}")
    add_ldx12_sample(ViewportScissor "${VIEWPORT_SCISSOR_SOURCE}")
    add_ldx12_sample(TextureSamplers "${TEXTURE_SAMPLERS_SOURCE}")
    add_ldx12_sample(CBSRVCubes "${CBSRVCUBES_SOURCE}" "${CBSRVCUBES_README}" "${CBSRVCUBES_SHADER}")
    add_ldx12_sample(CookbookChapter02 "${COOKBOOK_CHAPTER02_SOURCE}" "${COOKBOOK_CHAPTER02_README}" "${COOKBOOK_CHAPTER02_SHADER}")

    set(IMGUI_DEMO_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/samples/ImGuiDemo/main.cpp")
    add_executable(ImGuiDemo WIN32 "${IMGUI_DEMO_SOURCE}")
    target_compile_features(ImGuiDemo PRIVATE cxx_std_20)
    target_link_libraries(ImGuiDemo PRIVATE Ldx12::ImGui)
    if(MSVC)
        target_compile_options(ImGuiDemo PRIVATE /W4 /EHsc /permissive- /MP)
    endif()

    set(IMGUI_DEMO_NATIVE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/samples/ImGuiDemoNative/main.cpp")
    add_executable(ImGuiDemoNative WIN32 "${IMGUI_DEMO_NATIVE_SOURCE}")
    target_compile_features(ImGuiDemoNative PRIVATE cxx_std_20)
    target_link_libraries(ImGuiDemoNative PRIVATE DearImGui)
    if(MSVC)
        target_compile_options(ImGuiDemoNative PRIVATE /W4 /EHsc /permissive- /MP)
    endif()

    set(LDX12_EXAMPLE_TARGETS Triangle ViewportScissor TextureSamplers CBSRVCubes CookbookChapter02 ImGuiDemo ImGuiDemoNative)
    set(LDX12_EXAMPLE_SOURCES "${TRIANGLE_SOURCE}" "${VIEWPORT_SCISSOR_SOURCE}" "${TEXTURE_SAMPLERS_SOURCE}" "${CBSRVCUBES_SOURCE}" "${CBSRVCUBES_README}" "${CBSRVCUBES_SHADER}" "${COOKBOOK_CHAPTER02_SOURCE}" "${COOKBOOK_CHAPTER02_README}" "${COOKBOOK_CHAPTER02_SHADER}" "${IMGUI_DEMO_SOURCE}" "${IMGUI_DEMO_NATIVE_SOURCE}")

    if(LDX12_BUILD_APP)
        set(IMGUI_NODE_EDITOR_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/samples/ImGuiNodeEditor/main.cpp")
        set(IMGUI_NODE_EDITOR_README "${CMAKE_CURRENT_SOURCE_DIR}/samples/ImGuiNodeEditor/README.md")
        add_executable(ImGuiNodeEditor WIN32 "${IMGUI_NODE_EDITOR_SOURCE}" "${IMGUI_NODE_EDITOR_README}")
        target_compile_features(ImGuiNodeEditor PRIVATE cxx_std_20)
        target_link_libraries(ImGuiNodeEditor PRIVATE Ldx12::Ldx12 Ldx12::App DearImGui)
        if(MSVC)
            target_compile_options(ImGuiNodeEditor PRIVATE /W4 /EHsc /permissive- /MP)
        endif()
        list(APPEND LDX12_EXAMPLE_TARGETS ImGuiNodeEditor)
        list(APPEND LDX12_EXAMPLE_SOURCES "${IMGUI_NODE_EDITOR_SOURCE}" "${IMGUI_NODE_EDITOR_README}")
    endif()

    set_target_properties(${LDX12_EXAMPLE_TARGETS} PROPERTIES FOLDER "examples")
    set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT Triangle)
endif()

set(LDX12_TEST_SOURCES)
if(LDX12_BUILD_TESTS)
    set(LDX12_CORE_TEST_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/Ldx12Tests.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/Ldx12/TestTemplate.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/Ldx12/SlotMapCreationTest.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/Ldx12/SlotMapDestroyTest.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/Ldx12/SlotMapStaleHandleTest.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/Ldx12/SlotMapCapacityTest.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/Ldx12/SlotMapVirtualObjectTest.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/Ldx12/PublicArrayTest.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/Ldx12/GpuResourceLifecycleTest.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/Ldx12/GpuInvalidHandleTest.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/Ldx12/GpuSubmissionTest.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/Ldx12/GpuDescriptorRecyclingTest.cpp"
    )
    add_executable(Ldx12Tests ${LDX12_CORE_TEST_SOURCES})
    target_compile_features(Ldx12Tests PRIVATE cxx_std_20)
    target_link_libraries(Ldx12Tests PRIVATE Ldx12::Ldx12)
    if(MSVC)
        target_compile_options(Ldx12Tests PRIVATE /W4 /EHsc /permissive- /MP)
    endif()
    add_test(NAME Ldx12Tests COMMAND Ldx12Tests)
    set_tests_properties(Ldx12Tests PROPERTIES TIMEOUT 60)

    set(LDX12_STRESS_TEST_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/tests/Ldx12StressTests.cpp")
    add_executable(Ldx12StressTests "${LDX12_STRESS_TEST_SOURCE}")
    target_compile_features(Ldx12StressTests PRIVATE cxx_std_20)
    target_link_libraries(Ldx12StressTests PRIVATE Ldx12::Ldx12)
    if(MSVC)
        target_compile_options(Ldx12StressTests PRIVATE /W4 /EHsc /permissive- /MP)
    endif()
    add_test(NAME Ldx12StressTests COMMAND Ldx12StressTests)
    set_tests_properties(Ldx12StressTests PROPERTIES TIMEOUT 120)

    set(LDX12_FRAME_STRESS_TEST_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/tests/Ldx12FrameStressTests.cpp")
    add_executable(Ldx12FrameStressTests "${LDX12_FRAME_STRESS_TEST_SOURCE}")
    target_compile_features(Ldx12FrameStressTests PRIVATE cxx_std_20)
    target_link_libraries(Ldx12FrameStressTests PRIVATE Ldx12::Ldx12)
    if(MSVC)
        target_compile_options(Ldx12FrameStressTests PRIVATE /W4 /EHsc /permissive- /MP)
    endif()
    add_test(NAME Ldx12FrameStressTests COMMAND Ldx12FrameStressTests)
    set_tests_properties(Ldx12FrameStressTests PROPERTIES TIMEOUT 180)

    set(LDX12_COMMAND_STREAM_STRESS_TEST_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/tests/Ldx12CommandStreamStressTests.cpp")
    add_executable(Ldx12CommandStreamStressTests "${LDX12_COMMAND_STREAM_STRESS_TEST_SOURCE}")
    target_compile_features(Ldx12CommandStreamStressTests PRIVATE cxx_std_20)
    target_link_libraries(Ldx12CommandStreamStressTests PRIVATE Ldx12::Ldx12)
    if(MSVC)
        target_compile_options(Ldx12CommandStreamStressTests PRIVATE /W4 /EHsc /permissive- /MP)
    endif()
    add_test(NAME Ldx12CommandStreamStressTests COMMAND Ldx12CommandStreamStressTests)
    set_tests_properties(Ldx12CommandStreamStressTests PROPERTIES TIMEOUT 180)

    set(LDX12_FULL_CAPACITY_TEST_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/tests/Ldx12FullCapacityTests.cpp")
    add_executable(Ldx12FullCapacityTests "${LDX12_FULL_CAPACITY_TEST_SOURCE}")
    target_compile_features(Ldx12FullCapacityTests PRIVATE cxx_std_20)
    target_link_libraries(Ldx12FullCapacityTests PRIVATE Ldx12::Ldx12)
    if(MSVC)
        target_compile_options(Ldx12FullCapacityTests PRIVATE /W4 /EHsc /permissive- /MP)
    endif()
    add_test(NAME Ldx12FullCapacityTests COMMAND Ldx12FullCapacityTests)
    set_tests_properties(Ldx12FullCapacityTests PROPERTIES TIMEOUT 600)

    set(LDX12_TEST_TARGETS Ldx12Tests Ldx12StressTests Ldx12FrameStressTests Ldx12CommandStreamStressTests Ldx12FullCapacityTests)
    set(LDX12_TEST_SOURCES ${LDX12_CORE_TEST_SOURCES} "${LDX12_STRESS_TEST_SOURCE}" "${LDX12_FRAME_STRESS_TEST_SOURCE}" "${LDX12_COMMAND_STREAM_STRESS_TEST_SOURCE}" "${LDX12_FULL_CAPACITY_TEST_SOURCE}")

    if(LDX12_BUILD_APP)
        set(NODE_GRAPH_TEST_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/tests/NodeGraphTests.cpp")
        add_executable(Ldx12NodeGraphTests "${NODE_GRAPH_TEST_SOURCE}")
        target_compile_features(Ldx12NodeGraphTests PRIVATE cxx_std_20)
        target_link_libraries(Ldx12NodeGraphTests PRIVATE Ldx12::App)
        if(MSVC)
            target_compile_options(Ldx12NodeGraphTests PRIVATE /W4 /EHsc /permissive- /MP)
        endif()
        add_test(NAME Ldx12NodeGraphTests COMMAND Ldx12NodeGraphTests)
        list(APPEND LDX12_TEST_TARGETS Ldx12NodeGraphTests)
        list(APPEND LDX12_TEST_SOURCES "${NODE_GRAPH_TEST_SOURCE}")
    endif()

    set_target_properties(${LDX12_TEST_TARGETS} PROPERTIES FOLDER "Folder_Test")
endif()

set_target_properties(Ldx12 PROPERTIES FOLDER "API")
if(LDX12_BUILD_APP)
    set_target_properties(App PROPERTIES FOLDER "App")
endif()

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES
    ${LDX12_PUBLIC_HEADERS}
    ${LDX12_PRIVATE_HEADERS}
    ${LDX12_SOURCES}
    ${LDX12_SHADERS}
    ${APP_PUBLIC_HEADERS}
    ${APP_SOURCES}
    ${LDX12_EXAMPLE_SOURCES}
    ${LDX12_TEST_SOURCES}
)
source_group("Generated" FILES ${LDX12_GENERATED_HEADERS})
