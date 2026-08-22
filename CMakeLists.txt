cmake_minimum_required(VERSION 3.24)

project(LightDX12 VERSION 0.1.0 LANGUAGES CXX)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
option(LIGHTDX12_BUILD_APP "Build the optional App support library" ${PROJECT_IS_TOP_LEVEL})
option(LIGHTDX12_BUILD_EXAMPLES "Build the LightDX12 examples" ${PROJECT_IS_TOP_LEVEL})
option(LIGHTDX12_BUILD_TESTS "Build the LightDX12 tests" ${PROJECT_IS_TOP_LEVEL})
option(LIGHTDX12_INSTALL "Generate installation rules for LightD3D12" ${PROJECT_IS_TOP_LEVEL})
if(LIGHTDX12_BUILD_TESTS)
    enable_testing()
endif()

if(NOT WIN32)
    message(FATAL_ERROR "LightD3D12 currently supports only Windows.")
endif()

include(GNUInstallDirs)

set(LIGHTD3D12_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/LightD3D12")
set(LIGHTD3D12_BASE_MIPS_SHADER "${LIGHTD3D12_ROOT}/src/shaders/LightD3D12BaseMipsCS.hlsl")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${LIGHTD3D12_BASE_MIPS_SHADER}")
file(READ "${LIGHTD3D12_BASE_MIPS_SHADER}" LIGHTD3D12_BASE_MIPS_SHADER_SOURCE)
set(LIGHTD3D12_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
file(MAKE_DIRECTORY "${LIGHTD3D12_GENERATED_DIR}")
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/LightD3D12BaseMipsShader.hpp.in"
    "${LIGHTD3D12_GENERATED_DIR}/LightD3D12BaseMipsShader.hpp"
    @ONLY
)

set(LIGHTD3D12_PUBLIC_HEADERS
    "${LIGHTD3D12_ROOT}/include/LightD3D12/HandleSlotMap.hpp"
    "${LIGHTD3D12_ROOT}/include/LightD3D12/LightD3D12.hpp"
    "${LIGHTD3D12_ROOT}/include/LightD3D12/LightD3D12_Defines.hpp"
    "${LIGHTD3D12_ROOT}/include/LightD3D12/LightHLSLLoader.hpp"
)

set(LIGHTD3D12_PRIVATE_HEADERS
    "${LIGHTD3D12_ROOT}/src/d3dx12.h"
    "${LIGHTD3D12_ROOT}/src/LightD3D12BaseMips.hpp"
    "${LIGHTD3D12_ROOT}/src/LightD3D12CommandBatch.hpp"
    "${LIGHTD3D12_ROOT}/src/LightD3D12CommandBuffer.hpp"
    "${LIGHTD3D12_ROOT}/src/LightD3D12ImmediateCommands.hpp"
    "${LIGHTD3D12_ROOT}/src/LightD3D12Internal.hpp"
    "${LIGHTD3D12_ROOT}/src/LightD3D12ManagerImpl.hpp"
    "${LIGHTD3D12_ROOT}/src/LightD3D12Resources.hpp"
    "${LIGHTD3D12_ROOT}/src/LightD3D12ShaderCompiler.hpp"
    "${LIGHTD3D12_ROOT}/src/LightD3D12StagingDevice.hpp"
    "${LIGHTD3D12_ROOT}/src/LightD3D12Swapchain.hpp"
)

set(LIGHTD3D12_GENERATED_HEADERS
    "${LIGHTD3D12_GENERATED_DIR}/LightD3D12BaseMipsShader.hpp"
)

set(LIGHTD3D12_SOURCES
    "${LIGHTD3D12_ROOT}/src/LightD3D12BaseMips.cpp"
    "${LIGHTD3D12_ROOT}/src/LightD3D12CommandBatch.cpp"
    "${LIGHTD3D12_ROOT}/src/LightD3D12CommandBuffer.cpp"
    "${LIGHTD3D12_ROOT}/src/LightD3D12DeviceManager.cpp"
    "${LIGHTD3D12_ROOT}/src/LightD3D12ImmediateCommands.cpp"
    "${LIGHTD3D12_ROOT}/src/LightD3D12RenderDevice.cpp"
    "${LIGHTD3D12_ROOT}/src/LightD3D12Resources.cpp"
    "${LIGHTD3D12_ROOT}/src/LightD3D12ShaderCompiler.cpp"
    "${LIGHTD3D12_ROOT}/src/LightD3D12StagingDevice.cpp"
    "${LIGHTD3D12_ROOT}/src/LightD3D12Swapchain.cpp"
    "${LIGHTD3D12_ROOT}/src/LightHLSLLoader.cpp"
)

set(LIGHTD3D12_SHADERS
    "${LIGHTD3D12_BASE_MIPS_SHADER}"
    "${LIGHTD3D12_ROOT}/src/shaders/LightD3D12_Defines.hlsli"
)

set_source_files_properties(${LIGHTD3D12_SHADERS} PROPERTIES HEADER_FILE_ONLY TRUE)

add_library(LightD3D12 STATIC
    ${LIGHTD3D12_PUBLIC_HEADERS}
    ${LIGHTD3D12_PRIVATE_HEADERS}
    ${LIGHTD3D12_GENERATED_HEADERS}
    ${LIGHTD3D12_SOURCES}
    ${LIGHTD3D12_SHADERS}
)

add_library(LightD3D12::LightD3D12 ALIAS LightD3D12)

target_compile_features(LightD3D12 PUBLIC cxx_std_20)
target_compile_definitions(LightD3D12
    PUBLIC
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        UNICODE
        _UNICODE
)
target_include_directories(LightD3D12
    PUBLIC
        "$<BUILD_INTERFACE:${LIGHTD3D12_ROOT}/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
    PRIVATE
        "${LIGHTD3D12_ROOT}/src"
        "${LIGHTD3D12_GENERATED_DIR}"
)
target_link_libraries(LightD3D12
    PUBLIC
        d3d12
        d3dcompiler
        dxgi
        dxguid
)

if(MSVC)
    target_compile_options(LightD3D12 PRIVATE /W4 /EHsc /permissive- /MP)
endif()

if(LIGHTDX12_INSTALL)
    include(CMakePackageConfigHelpers)
    set(LIGHTD3D12_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/LightD3D12")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/cmake")
    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/LightD3D12Config.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake/LightD3D12Config.cmake"
        INSTALL_DESTINATION "${LIGHTD3D12_INSTALL_CMAKEDIR}"
    )
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/cmake/LightD3D12ConfigVersion.cmake"
        VERSION "${PROJECT_VERSION}"
        COMPATIBILITY SameMajorVersion
    )
    install(
        TARGETS LightD3D12
        EXPORT LightD3D12Targets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    )
    install(DIRECTORY "${LIGHTD3D12_ROOT}/include/" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
    install(
        EXPORT LightD3D12Targets
        FILE LightD3D12Targets.cmake
        NAMESPACE LightD3D12::
        DESTINATION "${LIGHTD3D12_INSTALL_CMAKEDIR}"
    )
    install(
        FILES
            "${CMAKE_CURRENT_BINARY_DIR}/cmake/LightD3D12Config.cmake"
            "${CMAKE_CURRENT_BINARY_DIR}/cmake/LightD3D12ConfigVersion.cmake"
        DESTINATION "${LIGHTD3D12_INSTALL_CMAKEDIR}"
    )
endif()

set(APP_PUBLIC_HEADERS)
set(APP_SOURCES)
if(LIGHTDX12_BUILD_APP)
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

    set(APP_PUBLIC_HEADERS
        "${CMAKE_CURRENT_SOURCE_DIR}/App/include/App/ImGuiLayer.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/App/include/App/NodeGraph.hpp"
    )
    set(APP_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/App/src/ImGuiLayer.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/App/src/NodeGraph.cpp"
    )
    add_library(App STATIC ${APP_PUBLIC_HEADERS} ${APP_SOURCES})
    add_library(LightDX12::App ALIAS App)
    target_compile_features(App PUBLIC cxx_std_20)
    target_include_directories(App PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/App/include")
    target_compile_definitions(App PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    target_link_libraries(App PRIVATE DearImGui)
    if(MSVC)
        target_compile_options(App PRIVATE /W4 /EHsc /permissive- /MP)
    endif()
endif()

set(LIGHTDX12_EXAMPLE_SOURCES)
if(LIGHTDX12_BUILD_EXAMPLES)
    function(add_lightd3d12_sample target_name)
        add_executable(${target_name} WIN32 ${ARGN})
        target_link_libraries(${target_name} PRIVATE LightD3D12::LightD3D12)
        target_compile_features(${target_name} PRIVATE cxx_std_20)
        if(MSVC)
            target_compile_options(${target_name} PRIVATE /W4 /EHsc /permissive- /MP)
        endif()
    endfunction()

    set(TRIANGLE_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/samples/Triangle/main.cpp")
    set(CBSRVCUBES_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/samples/CBSRVCubes/main.cpp")
    set(CBSRVCUBES_README "${CMAKE_CURRENT_SOURCE_DIR}/samples/CBSRVCubes/README.md")
    set(CBSRVCUBES_SHADER "${CMAKE_CURRENT_SOURCE_DIR}/samples/CBSRVCubes/shaders/CBSRVCubes.hlsl")
    set_source_files_properties("${CBSRVCUBES_SHADER}" PROPERTIES HEADER_FILE_ONLY TRUE)
    add_lightd3d12_sample(Triangle "${TRIANGLE_SOURCE}")
    add_lightd3d12_sample(CBSRVCubes "${CBSRVCUBES_SOURCE}" "${CBSRVCUBES_README}" "${CBSRVCUBES_SHADER}")
    set(LIGHTDX12_EXAMPLE_TARGETS Triangle CBSRVCubes)
    set(LIGHTDX12_EXAMPLE_SOURCES "${TRIANGLE_SOURCE}" "${CBSRVCUBES_SOURCE}" "${CBSRVCUBES_README}" "${CBSRVCUBES_SHADER}")

    if(LIGHTDX12_BUILD_APP)
        set(IMGUI_NODE_EDITOR_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/samples/ImGuiNodeEditor/main.cpp")
        set(IMGUI_NODE_EDITOR_README "${CMAKE_CURRENT_SOURCE_DIR}/samples/ImGuiNodeEditor/README.md")
        add_executable(ImGuiNodeEditor WIN32 "${IMGUI_NODE_EDITOR_SOURCE}" "${IMGUI_NODE_EDITOR_README}")
        target_compile_features(ImGuiNodeEditor PRIVATE cxx_std_20)
        target_link_libraries(ImGuiNodeEditor PRIVATE LightD3D12::LightD3D12 LightDX12::App DearImGui)
        if(MSVC)
            target_compile_options(ImGuiNodeEditor PRIVATE /W4 /EHsc /permissive- /MP)
        endif()
        list(APPEND LIGHTDX12_EXAMPLE_TARGETS ImGuiNodeEditor)
        list(APPEND LIGHTDX12_EXAMPLE_SOURCES "${IMGUI_NODE_EDITOR_SOURCE}" "${IMGUI_NODE_EDITOR_README}")
    endif()

    set_target_properties(${LIGHTDX12_EXAMPLE_TARGETS} PROPERTIES FOLDER "examples")
    set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT Triangle)
endif()

set(LIGHTDX12_TEST_SOURCES)
if(LIGHTDX12_BUILD_TESTS)
    set(LIGHTDX12_CORE_TEST_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/LightDX12Tests.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/LightDX12/TestTemplate.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/LightDX12/SlotMapCreationTest.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/LightDX12/SlotMapDestroyTest.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/LightDX12/SlotMapStaleHandleTest.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/LightDX12/SlotMapCapacityTest.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/LightDX12/SlotMapVirtualObjectTest.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/LightDX12/PublicArrayTest.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/LightDX12/GpuResourceLifecycleTest.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/LightDX12/GpuInvalidHandleTest.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/LightDX12/GpuSubmissionTest.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/LightDX12/GpuDescriptorRecyclingTest.cpp"
    )
    add_executable(LightDX12Tests ${LIGHTDX12_CORE_TEST_SOURCES})
    target_compile_features(LightDX12Tests PRIVATE cxx_std_20)
    target_link_libraries(LightDX12Tests PRIVATE LightD3D12::LightD3D12)
    if(MSVC)
        target_compile_options(LightDX12Tests PRIVATE /W4 /EHsc /permissive- /MP)
    endif()
    add_test(NAME LightDX12Tests COMMAND LightDX12Tests)
    set_tests_properties(LightDX12Tests PROPERTIES TIMEOUT 60)
    set(LIGHTDX12_TEST_TARGETS LightDX12Tests)
    set(LIGHTDX12_TEST_SOURCES ${LIGHTDX12_CORE_TEST_SOURCES})

    if(LIGHTDX12_BUILD_APP)
        set(NODE_GRAPH_TEST_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/tests/NodeGraphTests.cpp")
        add_executable(LightDX12NodeGraphTests "${NODE_GRAPH_TEST_SOURCE}")
        target_compile_features(LightDX12NodeGraphTests PRIVATE cxx_std_20)
        target_link_libraries(LightDX12NodeGraphTests PRIVATE LightDX12::App)
        if(MSVC)
            target_compile_options(LightDX12NodeGraphTests PRIVATE /W4 /EHsc /permissive- /MP)
        endif()
        add_test(NAME LightDX12NodeGraphTests COMMAND LightDX12NodeGraphTests)
        list(APPEND LIGHTDX12_TEST_TARGETS LightDX12NodeGraphTests)
        list(APPEND LIGHTDX12_TEST_SOURCES "${NODE_GRAPH_TEST_SOURCE}")
    endif()

    set_target_properties(${LIGHTDX12_TEST_TARGETS} PROPERTIES FOLDER "Folder_Test")
endif()

set_target_properties(LightD3D12 PROPERTIES FOLDER "API")
if(LIGHTDX12_BUILD_APP)
    set_target_properties(App PROPERTIES FOLDER "App")
endif()

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES
    ${LIGHTD3D12_PUBLIC_HEADERS}
    ${LIGHTD3D12_PRIVATE_HEADERS}
    ${LIGHTD3D12_SOURCES}
    ${LIGHTD3D12_SHADERS}
    ${APP_PUBLIC_HEADERS}
    ${APP_SOURCES}
    ${LIGHTDX12_EXAMPLE_SOURCES}
    ${LIGHTDX12_TEST_SOURCES}
)
source_group("Generated" FILES ${LIGHTD3D12_GENERATED_HEADERS})
