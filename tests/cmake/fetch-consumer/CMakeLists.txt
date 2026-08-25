cmake_minimum_required(VERSION 3.24)

project(Ldx12FetchContentConsumer LANGUAGES CXX)

include(FetchContent)

set(LDX12_BUILD_APP OFF CACHE BOOL "" FORCE)
set(LDX12_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LDX12_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(LDX12_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    Ldx12
    GIT_REPOSITORY https://github.com/alfonsmagd/LightDX12.git
    GIT_TAG main
)
FetchContent_MakeAvailable(Ldx12)

add_executable(Ldx12FetchContentConsumer "../main.cpp")
target_link_libraries(Ldx12FetchContentConsumer PRIVATE Ldx12::Ldx12)

enable_testing()
add_test(NAME Ldx12FetchContentConsumer COMMAND Ldx12FetchContentConsumer)
