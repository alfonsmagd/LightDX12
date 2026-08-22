cmake_minimum_required(VERSION 3.24)

project(Ldx12SourceConsumer LANGUAGES CXX)

add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../../.." Ldx12 EXCLUDE_FROM_ALL)

foreach(option_name IN ITEMS LDX12_BUILD_APP LDX12_BUILD_EXAMPLES LDX12_BUILD_TESTS LDX12_INSTALL)
    if(${option_name})
        message(FATAL_ERROR "${option_name} must default to OFF when Ldx12 is a subproject.")
    endif()
endforeach()

if(NOT TARGET Ldx12::Ldx12)
    message(FATAL_ERROR "The Ldx12::Ldx12 target was not generated.")
endif()

foreach(unexpected_target IN ITEMS DearImGui App Triangle CBSRVCubes ImGuiNodeEditor Ldx12Tests Ldx12NodeGraphTests)
    if(TARGET ${unexpected_target})
        message(FATAL_ERROR "Unexpected optional target generated: ${unexpected_target}")
    endif()
endforeach()

add_executable(Ldx12SourceConsumer "../main.cpp")
target_link_libraries(Ldx12SourceConsumer PRIVATE Ldx12::Ldx12)

enable_testing()
add_test(NAME Ldx12SourceConsumer COMMAND Ldx12SourceConsumer)
