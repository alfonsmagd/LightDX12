add_library(GltfSampleCommon INTERFACE)
target_sources(GltfSampleCommon PRIVATE
    SceneMaterials.hpp SceneEnvironment.hpp SceneTargets.hpp GltfSample.manifest)
target_include_directories(GltfSampleCommon INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
set_target_properties(GltfSampleCommon PROPERTIES FOLDER "ThirdParty")
source_group(TREE "${LDX12_PROJECT_ROOT}" FILES ${CMAKE_CURRENT_SOURCE_DIR}/SceneMaterials.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/SceneEnvironment.hpp ${CMAKE_CURRENT_SOURCE_DIR}/SceneTargets.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/GltfSample.manifest)
