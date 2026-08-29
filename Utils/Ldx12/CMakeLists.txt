cmake_minimum_required(VERSION 3.24)

project(Ldx12Utils LANGUAGES CXX)

if(PROJECT_IS_TOP_LEVEL)
    include(GNUInstallDirs)
    find_package(Ldx12 CONFIG REQUIRED)
endif()

set(LDX12_UTILS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(LDX12_UTILS_PUBLIC_HEADERS
    "${LDX12_UTILS_ROOT}/include/Ldx12Utils/Ldx12Utils.hpp"
    "${LDX12_UTILS_ROOT}/include/Ldx12Utils/AppLdx.hpp"
    "${LDX12_UTILS_ROOT}/include/Ldx12Utils/DepthTarget.hpp"
    "${LDX12_UTILS_ROOT}/include/Ldx12Utils/Geometry.hpp"
    "${LDX12_UTILS_ROOT}/include/Ldx12Utils/OrbitCamera.hpp"
    "${LDX12_UTILS_ROOT}/include/Ldx12Utils/TextureLoader.hpp"
)
set(LDX12_UTILS_SOURCES
    "${LDX12_UTILS_ROOT}/src/Ldx12Utils.cpp"
    "${LDX12_UTILS_ROOT}/src/AppLdx.cpp"
    "${LDX12_UTILS_ROOT}/src/DepthTarget.cpp"
    "${LDX12_UTILS_ROOT}/src/Geometry.cpp"
    "${LDX12_UTILS_ROOT}/src/OrbitCamera.cpp"
    "${LDX12_UTILS_ROOT}/src/TextureLoader.cpp"
)

add_library(Ldx12Utils STATIC
    ${LDX12_UTILS_PUBLIC_HEADERS}
    ${LDX12_UTILS_SOURCES}
)
add_library(Ldx12::Utils ALIAS Ldx12Utils)

set_target_properties(Ldx12Utils PROPERTIES DEBUG_POSTFIX "d" EXPORT_NAME Utils)
target_compile_features(Ldx12Utils PUBLIC cxx_std_20)
target_include_directories(Ldx12Utils
    PUBLIC
        "$<BUILD_INTERFACE:${LDX12_UTILS_ROOT}/include>"
        "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
)
target_link_libraries(Ldx12Utils
    PUBLIC Ldx12::Ldx12
    PRIVATE windowscodecs ole32
)

if(MSVC)
    target_compile_options(Ldx12Utils PRIVATE /W4 /EHsc /permissive- /MP)
endif()

if(NOT PROJECT_IS_TOP_LEVEL)
    set(LDX12_UTILS_PUBLIC_HEADERS ${LDX12_UTILS_PUBLIC_HEADERS} PARENT_SCOPE)
    set(LDX12_UTILS_SOURCES ${LDX12_UTILS_SOURCES} PARENT_SCOPE)
endif()
