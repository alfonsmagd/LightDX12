cmake_minimum_required(VERSION 3.24)

project(Ldx12InstalledExample LANGUAGES CXX)

option(LDX12_USE_FETCHCONTENT "Download Ldx12 instead of using an installed package" ON)
set(LDX12_FETCH_GIT_TAG "main" CACHE STRING "Ldx12 Git tag or commit used by FetchContent")

if(LDX12_USE_FETCHCONTENT)
    include(FetchContent)
    set(LDX12_BUILD_APP OFF CACHE BOOL "" FORCE)
    set(LDX12_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(LDX12_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(LDX12_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        Ldx12
        GIT_REPOSITORY https://github.com/alfonsmagd/LightDX12.git
        GIT_TAG "${LDX12_FETCH_GIT_TAG}"
    )
    FetchContent_MakeAvailable(Ldx12)
else()
    find_package(Ldx12 CONFIG REQUIRED)
endif()

add_executable(Ldx12InstalledExample main.cpp)
target_link_libraries(Ldx12InstalledExample PRIVATE Ldx12::Ldx12)

enable_testing()
add_test(NAME Ldx12InstalledExample COMMAND Ldx12InstalledExample)
