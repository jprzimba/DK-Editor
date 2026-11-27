cmake_minimum_required(VERSION 3.20)
project(DKEditor LANGUAGES C CXX)

if (MSVC)
  enable_language(RC)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MSVC)
  add_compile_options(/utf-8 /W3 /permissive- /MP)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS NOMINMAX)
endif()

# Dear ImGui
add_library(imgui STATIC
  third_party/imgui/imgui.cpp
  third_party/imgui/imgui_draw.cpp
  third_party/imgui/imgui_tables.cpp
  third_party/imgui/imgui_widgets.cpp
  third_party/imgui/imgui_demo.cpp
  third_party/imgui/backends/imgui_impl_win32.cpp
  third_party/imgui/backends/imgui_impl_dx11.cpp
)
target_include_directories(imgui PUBLIC third_party/imgui third_party/imgui/backends)
target_link_libraries(imgui PUBLIC d3d11 dxgi d3dcompiler)

# Kingizor's MIT-licensed compression library
file(GLOB DKCOMP_SOURCES third_party/dkcomp/*.c)
add_library(dkcomp STATIC ${DKCOMP_SOURCES})
target_include_directories(dkcomp PUBLIC third_party/dkcomp)
target_compile_definitions(dkcomp PUBLIC BUILD_DKCOMP)
if (MSVC)
  target_compile_options(dkcomp PRIVATE /W0)
endif()

set(DKEDITOR_GAME
  src/core/Util.cpp
  src/core/Rom.cpp
  src/core/Compression.cpp
  src/core/Graphics.cpp
  src/games/Dkc1.cpp
  src/games/Dkc2.cpp
  src/games/Dkc3.cpp
  src/games/Games.cpp
  src/games/SpriteData.cpp
)

set(DKEDITOR_UI
  src/ui/Texture.cpp
  src/ui/EditorApp.cpp
)

add_executable(DKEditor WIN32
  src/main.cpp
  ${DKEDITOR_GAME}
  ${DKEDITOR_UI}
  src/resources/DKEditor.rc
)
target_include_directories(DKEditor PRIVATE src third_party/imgui third_party/dkcomp src/resources)
target_compile_definitions(DKEditor PRIVATE IMGUI_DEFINE_MATH_OPERATORS)
target_link_libraries(DKEditor PRIVATE imgui dkcomp comdlg32 shell32)

add_executable(DKEditorSelftest src/selftest.cpp ${DKEDITOR_GAME})
target_include_directories(DKEditorSelftest PRIVATE src third_party/dkcomp)
target_link_libraries(DKEditorSelftest PRIVATE dkcomp)

if (MSVC)
  set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT DKEditor)
endif()
