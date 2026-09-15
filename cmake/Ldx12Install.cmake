include(CMakePackageConfigHelpers)
set(LDX12_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/Ldx12")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/cmake")
configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Ldx12Config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake/Ldx12Config.cmake"
    INSTALL_DESTINATION "${LDX12_INSTALL_CMAKEDIR}"
)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/cmake/Ldx12ConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMajorVersion
)
if(LDX12_BUILD_UTILS)
    install(
        TARGETS Ldx12 Ldx12Utils
        EXPORT Ldx12Targets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    )
    if(LDX12_BUILD_THIRD_PARTY)
        install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Utils/Ldx12/include/" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
    else()
        install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Utils/Ldx12/include/" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
            PATTERN "GltfLoader.hpp" EXCLUDE)
    endif()
else()
    install(
        TARGETS Ldx12
        EXPORT Ldx12Targets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    )
endif()
install(DIRECTORY "${LDX12_ROOT}/include/" DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(
    FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
        "${CMAKE_CURRENT_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
    DESTINATION "${CMAKE_INSTALL_DATADIR}/Ldx12"
)
install(
    EXPORT Ldx12Targets
    FILE Ldx12Targets.cmake
    NAMESPACE Ldx12::
    DESTINATION "${LDX12_INSTALL_CMAKEDIR}"
)
install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/cmake/Ldx12Config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/cmake/Ldx12ConfigVersion.cmake"
    DESTINATION "${LDX12_INSTALL_CMAKEDIR}"
)
