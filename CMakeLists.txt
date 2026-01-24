cmake_minimum_required(VERSION 3.10)
project(postframe-logic)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(OpenGL_GL_PREFERENCE GLVND)

# Поиск зависимостей
find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)

# Пути к заголовочным файлам
include_directories(
    deps/include 
    external/imgui
    external/imgui/backends
)

# Собираем список всех исходников
set(IMGUI_SOURCES
    external/imgui/imgui.cpp
    external/imgui/imgui_draw.cpp
    external/imgui/imgui_widgets.cpp
    external/imgui/imgui_tables.cpp
    external/imgui/imgui_demo.cpp
    external/imgui/backends/imgui_impl_glfw.cpp
    external/imgui/backends/imgui_impl_opengl3.cpp
)

# Создаем исполняемый файл
add_executable(${PROJECT_NAME} 
    src/main.cpp 
    src/Shader.cpp
    src/Texture.cpp
    src/TinyGltfImpl.cpp
    src/Framebuffer.cpp
    deps/src/gl.c
    ${IMGUI_SOURCES}
)

# Линковка библиотек
target_link_libraries(${PROJECT_NAME} 
    glfw 
    OpenGL::GL
)

# 1. Указываем CMake, где искать заголовочные файлы
target_include_directories(${PROJECT_NAME} PRIVATE 
    "src"
    "deps/include"
    "external/imgui"
    "include"
    "."
)

# Копируем всю папку assets в папку сборки после каждой компиляции
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/assets"
    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
    COMMENT "Copying assets to build..."
)