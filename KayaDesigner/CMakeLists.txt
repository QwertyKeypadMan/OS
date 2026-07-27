cmake_minimum_required(VERSION 3.22)

project(KayaDesigner LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(KAYA_DESIGNER_IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui" CACHE PATH "Dear ImGui source directory")

find_package(SDL3 CONFIG REQUIRED)

if(NOT EXISTS "${KAYA_DESIGNER_IMGUI_DIR}/imgui.cpp")
    message(FATAL_ERROR "Dear ImGui not found. Put imgui sources in KayaDesigner/third_party/imgui or set KAYA_DESIGNER_IMGUI_DIR.")
endif()

add_library(imgui STATIC
    "${KAYA_DESIGNER_IMGUI_DIR}/imgui.cpp"
    "${KAYA_DESIGNER_IMGUI_DIR}/imgui_draw.cpp"
    "${KAYA_DESIGNER_IMGUI_DIR}/imgui_tables.cpp"
    "${KAYA_DESIGNER_IMGUI_DIR}/imgui_widgets.cpp"
    "${KAYA_DESIGNER_IMGUI_DIR}/backends/imgui_impl_sdl3.cpp"
    "${KAYA_DESIGNER_IMGUI_DIR}/backends/imgui_impl_sdlrenderer3.cpp"
)

target_include_directories(imgui PUBLIC
    "${KAYA_DESIGNER_IMGUI_DIR}"
    "${KAYA_DESIGNER_IMGUI_DIR}/backends"
)

target_link_libraries(imgui PUBLIC SDL3::SDL3)

add_executable(KayaDesigner
    src/main.cpp
    src/App.cpp
    src/core/Logger.cpp
    src/document/GuiDocument.cpp
    src/document/WidgetRegistry.cpp
    src/editor/Editor.cpp
    src/ui/Canvas.cpp
    src/ui/HierarchyPanel.cpp
    src/ui/MainMenuBar.cpp
    src/ui/OutputPanel.cpp
    src/ui/PropertyGrid.cpp
    src/ui/Toolbar.cpp
    src/ui/WidgetPalette.cpp
    src/codegen/CCodeGenerator.cpp
)

target_include_directories(KayaDesigner PRIVATE src)
target_link_libraries(KayaDesigner PRIVATE imgui SDL3::SDL3)

if(MSVC)
    target_compile_options(KayaDesigner PRIVATE /W4 /permissive-)
else()
    target_compile_options(KayaDesigner PRIVATE -Wall -Wextra -Wpedantic)
endif()

