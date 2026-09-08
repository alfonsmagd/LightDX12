set_target_properties(Ldx12 PROPERTIES FOLDER "API")
if(LDX12_UTILS_AVAILABLE)
    set_target_properties(Ldx12Utils PROPERTIES FOLDER "Utils")
endif()
if(LDX12_BUILD_APP)
    set_target_properties(App PROPERTIES FOLDER "App")
endif()

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES
    ${LDX12_PUBLIC_HEADERS}
    ${LDX12_PRIVATE_HEADERS}
    ${LDX12_SOURCES}
    ${LDX12_SHADERS}
    ${LDX12_UTILS_PUBLIC_HEADERS}
    ${LDX12_UTILS_SOURCES}
    ${APP_PUBLIC_HEADERS}
    ${APP_SOURCES}
)
source_group("Generated" FILES ${LDX12_GENERATED_HEADERS})
