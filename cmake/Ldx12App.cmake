set(APP_PUBLIC_HEADERS)
set(APP_SOURCES)
if(LDX12_BUILD_APP OR LDX12_BUILD_EXAMPLES)
    set(IMGUI_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui")
    add_library(DearImGui STATIC
        "${IMGUI_ROOT}/imconfig.h"
        "${IMGUI_ROOT}/imgui.h"
        "${IMGUI_ROOT}/imgui_internal.h"
        "${IMGUI_ROOT}/imgui.cpp"
        "${IMGUI_ROOT}/imgui_draw.cpp"
        "${IMGUI_ROOT}/imgui_tables.cpp"
        "${IMGUI_ROOT}/imgui_widgets.cpp"
        "${IMGUI_ROOT}/imstb_rectpack.h"
        "${IMGUI_ROOT}/imstb_textedit.h"
        "${IMGUI_ROOT}/imstb_truetype.h"
        "${IMGUI_ROOT}/backends/imgui_impl_win32.h"
        "${IMGUI_ROOT}/backends/imgui_impl_win32.cpp"
        "${IMGUI_ROOT}/backends/imgui_impl_dx12.h"
        "${IMGUI_ROOT}/backends/imgui_impl_dx12.cpp"
    )
    target_compile_features(DearImGui PUBLIC cxx_std_20)
    target_include_directories(DearImGui PUBLIC "${IMGUI_ROOT}")
    target_compile_definitions(DearImGui PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    target_link_libraries(DearImGui PUBLIC d3d12 dxgi)

    add_library(Ldx12ImGui STATIC
        "${CMAKE_CURRENT_SOURCE_DIR}/App/include/App/imgui_impl_ldx12.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/App/src/imgui_impl_ldx12.cpp"
    )
    add_library(Ldx12::ImGui ALIAS Ldx12ImGui)
    target_compile_features(Ldx12ImGui PUBLIC cxx_std_20)
    target_include_directories(Ldx12ImGui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/App/include")
    target_compile_definitions(Ldx12ImGui PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    target_link_libraries(Ldx12ImGui PUBLIC Ldx12::Ldx12 DearImGui)
    ldx12_enable_warnings(Ldx12ImGui)

endif()

if(LDX12_BUILD_APP)

    set(APP_PUBLIC_HEADERS
        "${CMAKE_CURRENT_SOURCE_DIR}/App/include/App/ImGuiLayer.hpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/App/include/App/NodeGraph.hpp"
    )
    set(APP_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/App/src/ImGuiLayer.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/App/src/NodeGraph.cpp"
    )
    add_library(App STATIC ${APP_PUBLIC_HEADERS} ${APP_SOURCES})
    add_library(Ldx12::App ALIAS App)
    target_compile_features(App PUBLIC cxx_std_20)
    target_include_directories(App PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/App/include")
    target_compile_definitions(App PUBLIC WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
    target_link_libraries(App PUBLIC Ldx12::ImGui)
    ldx12_enable_warnings(App)
endif()
