cmake_minimum_required(VERSION 3.24)

project(LightD3D12SourceConsumer LANGUAGES CXX)

add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../../.." LightDX12 EXCLUDE_FROM_ALL)

foreach(option_name IN ITEMS LIGHTDX12_BUILD_APP LIGHTDX12_BUILD_EXAMPLES LIGHTDX12_BUILD_TESTS LIGHTDX12_INSTALL)
    if(${option_name})
        message(FATAL_ERROR "${option_name} must default to OFF when LightDX12 is a subproject.")
    endif()
endforeach()

if(NOT TARGET LightD3D12::LightD3D12)
    message(FATAL_ERROR "The LightD3D12::LightD3D12 target was not generated.")
endif()

foreach(unexpected_target IN ITEMS DearImGui App Triangle CBSRVCubes ImGuiNodeEditor LightDX12Tests LightDX12NodeGraphTests)
    if(TARGET ${unexpected_target})
        message(FATAL_ERROR "Unexpected optional target generated: ${unexpected_target}")
    endif()
endforeach()

add_executable(LightD3D12SourceConsumer "../main.cpp")
target_link_libraries(LightD3D12SourceConsumer PRIVATE LightD3D12::LightD3D12)

enable_testing()
add_test(NAME LightD3D12SourceConsumer COMMAND LightD3D12SourceConsumer)
