cmake_minimum_required(VERSION 3.24)

project(LightDX12 LANGUAGES CXX)

if(NOT WIN32)
    message(FATAL_ERROR "LightD3D12 currently supports only Windows.")
endif()

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

if(MSVC)
    target_compile_options(LightD3D12 PRIVATE /W4 /EHsc /permissive- /MP)
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
)
