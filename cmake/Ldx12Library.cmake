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

ldx12_enable_warnings(Ldx12)
