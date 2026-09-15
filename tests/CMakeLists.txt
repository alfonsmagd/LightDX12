# Test targets stay close to their test sources. This file only selects optional suites.
function(ldx12_add_test target)
    cmake_parse_arguments(PARSE_ARGV 1 test "WARNINGS" "TIMEOUT" "SOURCES;LIBRARIES")
    if(test_UNPARSED_ARGUMENTS OR test_KEYWORDS_MISSING_VALUES)
        message(FATAL_ERROR "Invalid arguments for test ${target}")
    endif()
    add_executable(${target} ${test_SOURCES})
    target_link_libraries(${target} PRIVATE ${test_LIBRARIES})
    if(test_WARNINGS)
        target_compile_features(${target} PRIVATE cxx_std_20)
        ldx12_enable_warnings(${target})
    endif()
    set_target_properties(${target} PROPERTIES FOLDER "Folder_Test")
    add_test(NAME ${target} COMMAND ${target})
    if(DEFINED test_TIMEOUT)
        set_tests_properties(${target} PROPERTIES TIMEOUT ${test_TIMEOUT})
    endif()
    source_group(TREE "${LDX12_PROJECT_ROOT}" FILES ${test_SOURCES})
endfunction()

ldx12_add_test(Ldx12NativeImportTests
    SOURCES Ldx12NativeImportTests.cpp LIBRARIES Ldx12::Ldx12 TIMEOUT 60)
add_subdirectory(Ldx12)

ldx12_add_test(Ldx12DeferredTests
    SOURCES Ldx12DeferredTests.cpp LIBRARIES Ldx12::Ldx12 TIMEOUT 10 WARNINGS)
target_include_directories(Ldx12DeferredTests PRIVATE "${LDX12_PROJECT_ROOT}/Ldx12/src")

ldx12_add_test(Ldx12StressTests
    SOURCES Ldx12StressTests.cpp LIBRARIES Ldx12::Ldx12 TIMEOUT 120 WARNINGS)
ldx12_add_test(Ldx12FrameStressTests
    SOURCES Ldx12FrameStressTests.cpp LIBRARIES Ldx12::Ldx12 TIMEOUT 180 WARNINGS)
ldx12_add_test(Ldx12CommandStreamStressTests
    SOURCES Ldx12CommandStreamStressTests.cpp LIBRARIES Ldx12::Ldx12 TIMEOUT 180 WARNINGS)
ldx12_add_test(Ldx12FullCapacityTests
    SOURCES Ldx12FullCapacityTests.cpp LIBRARIES Ldx12::Ldx12 TIMEOUT 600 WARNINGS)

if(LDX12_UTILS_AVAILABLE)
    ldx12_add_test(Ldx12UtilsTests
        SOURCES Ldx12UtilsTests.cpp LIBRARIES Ldx12::Utils TIMEOUT 60 WARNINGS)
    if(LDX12_BUILD_THIRD_PARTY)
        ldx12_add_test(Ldx12GltfTests
            SOURCES Ldx12GltfTests.cpp LIBRARIES Ldx12::Utils TIMEOUT 60)
        target_compile_definitions(Ldx12GltfTests PRIVATE
            LDX12_GLTF_FIXTURE_DIRECTORY="${LDX12_PROJECT_ROOT}/media/gltf")
        ldx12_add_test(Ldx12GltfMaterialTests
            SOURCES Ldx12GltfMaterialTests.cpp LIBRARIES Ldx12::Utils TIMEOUT 60)
        target_compile_definitions(Ldx12GltfMaterialTests PRIVATE
            LDX12_GLTF_MEDIA_DIRECTORY="${LDX12_PROJECT_ROOT}/media"
            LDX12_GLTF_SAMPLE_DIRECTORY="${LDX12_PROJECT_ROOT}/samples/GltfScene")
    endif()
endif()

if(LDX12_BUILD_APP)
    ldx12_add_test(Ldx12NodeGraphTests
        SOURCES NodeGraphTests.cpp LIBRARIES Ldx12::App WARNINGS)
endif()
