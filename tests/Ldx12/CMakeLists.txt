set(ldx12_core_test_sources
    "${CMAKE_CURRENT_LIST_DIR}/../Ldx12Tests.cpp"
    TestTemplate.hpp
    SlotMapCreationTest.cpp
    SlotMapDestroyTest.cpp
    SlotMapStaleHandleTest.cpp
    SlotMapCapacityTest.cpp
    SlotMapVirtualObjectTest.cpp
    PublicArrayTest.cpp
    GpuResourceLifecycleTest.cpp
    GpuSamplerDeferredDestructionTest.cpp
    GpuComputeTest.cpp
    GpuInvalidHandleTest.cpp
    GpuSubmissionTest.cpp
    GpuDescriptorRecyclingTest.cpp
    GpuMultisampleTest.cpp
    GpuTextureViewTest.cpp)

ldx12_add_test(Ldx12Tests
    SOURCES ${ldx12_core_test_sources}
    LIBRARIES Ldx12::Ldx12
    TIMEOUT 60
    WARNINGS)
