cmake_minimum_required(VERSION 3.24)

project(LightDX12 LANGUAGES CXX)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
option(BUILD_TESTING "Build the LightDX12 test projects" ON)
if(BUILD_TESTING)
    enable_testing()
endif()

if(NOT WIN32)
    message(FATAL_ERROR "LightD3D12 currently supports only Windows.")
endif()

include(FetchContent)

# App::TaskSystem is the only layer that sees enkiTS. Pinning the revision keeps
# local and CI builds reproducible.
set(ENKITS_BUILD_C_INTERFACE OFF CACHE BOOL "" FORCE)
set(ENKITS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ENKITS_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ENKITS_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    enkiTS
    GIT_REPOSITORY https://github.com/dougbinks/enkiTS.git
    GIT_TAG e3329cc01aeb3dff205e0153bf0480277d1a3dbd
    GIT_SHALLOW FALSE
)
FetchContent_MakeAvailable(enkiTS)

set(LIGHTD3D12_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/LightD3D12")

set(LIGHTD3D12_PUBLIC_HEADERS
    "${LIGHTD3D12_ROOT}/include/LightD3D12/HandleSlotMap.hpp"
    "${LIGHTD3D12_ROOT}/include/LightD3D12/LightD3D12.hpp"
    "${LIGHTD3D12_ROOT}/include/LightD3D12/LightD3D12_Defines.hpp"
    "${LIGHTD3D12_ROOT}/include/LightD3D12/LightHLSLLoader.hpp"
)

set(LIGHTD3D12_PRIVATE_HEADERS
    "${LIGHTD3D12_ROOT}/src/d3dx12.h"
    "${LIGHTD3D12_ROOT}/src/LightD3D12BaseMips.hpp"
    "${LIGHTD3D12_ROOT}/src/LightD3D12CommandBuffer.hpp"
    "${LIGHTD3D12_ROOT}/src/LightD3D12ImmediateCommands.hpp"
    "${LIGHTD3D12_ROOT}/src/LightD3D12Internal.hpp"
    "${LIGHTD3D12_ROOT}/src/LightD3D12ManagerImpl.hpp"
    "${LIGHTD3D12_ROOT}/src/LightD3D12Resources.hpp"
    "${LIGHTD3D12_ROOT}/src/LightD3D12ShaderCompiler.hpp"
    "${LIGHTD3D12_ROOT}/src/LightD3D12StagingDevice.hpp"
    "${LIGHTD3D12_ROOT}/src/LightD3D12Swapchain.hpp"
)

set(LIGHTD3D12_SOURCES
    "${LIGHTD3D12_ROOT}/src/LightD3D12BaseMips.cpp"
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
    "${LIGHTD3D12_ROOT}/src/shaders/LightD3D12BaseMipsCS.hlsl"
    "${LIGHTD3D12_ROOT}/src/shaders/LightD3D12_Defines.hlsli"
)

set_source_files_properties(${LIGHTD3D12_SHADERS} PROPERTIES HEADER_FILE_ONLY TRUE)

add_library(LightD3D12 STATIC
    ${LIGHTD3D12_PUBLIC_HEADERS}
    ${LIGHTD3D12_PRIVATE_HEADERS}
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
    PUBLIC "${LIGHTD3D12_ROOT}/include"
    PRIVATE "${LIGHTD3D12_ROOT}/src"
)
target_link_libraries(LightD3D12
    PUBLIC
        d3d12
        d3dcompiler
        dxgi
        dxguid
)

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
    "${CMAKE_CURRENT_SOURCE_DIR}/App/include/App/ConcurrentQueue.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/App/include/App/GpuMemory.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/App/include/App/HttpClient.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/App/include/App/ImGuiLayer.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/App/include/App/MeshGenerationClient.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/App/include/App/NodeGraph.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/App/include/App/ObjMesh.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/App/include/App/OrbitCamera.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/App/include/App/TaskSystem.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/App/include/App/WindowApplication.hpp"
)
set(APP_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/App/src/GpuMemory.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/App/src/HttpClient.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/App/src/ImGuiLayer.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/App/src/MeshGenerationClient.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/App/src/NodeGraph.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/App/src/ObjMesh.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/App/src/OrbitCamera.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/App/src/TaskSystem.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/App/src/WindowApplication.cpp"
)

add_library(App STATIC ${APP_PUBLIC_HEADERS} ${APP_SOURCES})
add_library(LightDX12::App ALIAS App)
target_compile_features(App PUBLIC cxx_std_20)
target_include_directories(App PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/App/include")
target_compile_definitions(App PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
target_link_libraries(App
    PUBLIC LightD3D12::LightD3D12
    PRIVATE enkiTS DearImGui winhttp
)

if(MSVC)
    target_compile_options(LightD3D12 PRIVATE /W4 /EHsc /permissive- /MP)
    target_compile_options(App PRIVATE /W4 /EHsc /permissive- /MP)
endif()

function(add_lightd3d12_sample target_name)
    add_executable(${target_name} WIN32 ${ARGN})
    target_link_libraries(${target_name} PRIVATE LightD3D12::LightD3D12)
    target_compile_features(${target_name} PRIVATE cxx_std_20)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4 /EHsc /permissive- /MP)
    endif()
endfunction()

add_lightd3d12_sample(
    Triangle
    "${CMAKE_CURRENT_SOURCE_DIR}/samples/Triangle/main.cpp"
)

set(CBSRVCUBES_SHADER "${CMAKE_CURRENT_SOURCE_DIR}/samples/CBSRVCubes/shaders/CBSRVCubes.hlsl")
set_source_files_properties("${CBSRVCUBES_SHADER}" PROPERTIES HEADER_FILE_ONLY TRUE)
add_lightd3d12_sample(
    CBSRVCubes
    "${CMAKE_CURRENT_SOURCE_DIR}/samples/CBSRVCubes/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/samples/CBSRVCubes/README.md"
    "${CBSRVCUBES_SHADER}"
)

add_executable(ImGuiNodeEditor WIN32
    "${CMAKE_CURRENT_SOURCE_DIR}/samples/ImGuiNodeEditor/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/samples/ImGuiNodeEditor/README.md"
)
target_compile_features(ImGuiNodeEditor PRIVATE cxx_std_20)
target_link_libraries(ImGuiNodeEditor PRIVATE LightDX12::App DearImGui)
if(MSVC)
    target_compile_options(ImGuiNodeEditor PRIVATE /W4 /EHsc /permissive- /MP)
endif()

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

if(BUILD_TESTING)
	add_executable(LightDX12Tests ${LIGHTDX12_CORE_TEST_SOURCES})
	target_compile_features(LightDX12Tests PRIVATE cxx_std_20)
	target_link_libraries(LightDX12Tests PRIVATE LightD3D12::LightD3D12)
	if(MSVC)
		target_compile_options(LightDX12Tests PRIVATE /W4 /EHsc /permissive- /MP)
	endif()
	add_test(NAME LightDX12Tests COMMAND LightDX12Tests)
	set_tests_properties(LightDX12Tests PROPERTIES TIMEOUT 60)

    add_executable(LightDX12AppTests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/AppSmokeTests.cpp"
    )
    target_compile_features(LightDX12AppTests PRIVATE cxx_std_20)
    target_link_libraries(LightDX12AppTests PRIVATE LightDX12::App)
    if(MSVC)
        target_compile_options(LightDX12AppTests PRIVATE /W4 /EHsc /permissive- /MP)
    endif()
    add_test(NAME LightDX12AppTests COMMAND LightDX12AppTests)

	set_target_properties(LightDX12Tests LightDX12AppTests PROPERTIES
		FOLDER "Folder_Test"
	)
endif()

set_target_properties(LightD3D12 PROPERTIES FOLDER "API")
set_target_properties(App PROPERTIES FOLDER "App")
set_target_properties(Triangle CBSRVCubes ImGuiNodeEditor PROPERTIES
	FOLDER "examples"
)

set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT Triangle)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES
    ${LIGHTD3D12_PUBLIC_HEADERS}
    ${LIGHTD3D12_PRIVATE_HEADERS}
    ${LIGHTD3D12_SOURCES}
    ${LIGHTD3D12_SHADERS}
    "${CMAKE_CURRENT_SOURCE_DIR}/samples/Triangle/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/samples/CBSRVCubes/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/samples/CBSRVCubes/README.md"
    "${CBSRVCUBES_SHADER}"
    ${APP_PUBLIC_HEADERS}
    ${APP_SOURCES}
    "${CMAKE_CURRENT_SOURCE_DIR}/samples/ImGuiNodeEditor/main.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/samples/ImGuiNodeEditor/README.md"
	${LIGHTDX12_CORE_TEST_SOURCES}
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/AppSmokeTests.cpp"
)
