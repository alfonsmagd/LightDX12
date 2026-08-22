cmake_minimum_required(VERSION 3.24)

project(LightD3D12InstallConsumer LANGUAGES CXX)

find_package(LightD3D12 CONFIG REQUIRED)

add_executable(LightD3D12InstallConsumer "../main.cpp")
target_link_libraries(LightD3D12InstallConsumer PRIVATE LightD3D12::LightD3D12)

enable_testing()
add_test(NAME LightD3D12InstallConsumer COMMAND LightD3D12InstallConsumer)
