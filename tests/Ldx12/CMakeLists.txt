set(ldx12_core_test_sources
    "${CMAKE_CURRENT_LIST_DIR}/../Ldx12Tests.cpp"
    TestTemplate.hpp
    SlotMapCreationTest.cpp
    SlotMapCapacityTest.cpp
    SlotMapFragmentationTest.cpp
    SlotMapVirtualObjectTest.cpp
    AdapterSelectionTest.cpp
    GpuResourceLifecycleTest.cpp
    GpuComputeTest.cpp
    GpuSubmissionTest.cpp
    GpuDescriptorRecyclingTest.cpp
    GpuMultisampleTest.cpp
    GpuTextureViewTest.cpp)

ldx12_add_test(Ldx12Tests
    SOURCES ${ldx12_core_test_sources}
    LIBRARIES Ldx12::Ldx12
    TIMEOUT 60
    WARNINGS)
