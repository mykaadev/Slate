# ========================================================================
# Minimum required version of CMake
# ========================================================================
cmake_minimum_required(VERSION 3.15)

# ========================================================================
# Centralized project metadata
# ========================================================================
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(AppMetadata)

# ========================================================================
# Project Info
# ========================================================================
project(${SOFTWARE_TARGET_NAME} VERSION ${SOFTWARE_APP_VERSION} LANGUAGES CXX)

# Use modern C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set binary output directory
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Set OpenGL to prefer legacy (fixed-function pipeline)
set(OpenGL_GL_PREFERENCE "LEGACY")

# ========================================================================
# Dependencies
# ========================================================================
include(FetchContent)

# -------------------------
# GLFW (Window + Input)
# -------------------------
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)
FetchContent_MakeAvailable(glfw)

# -------------------------
# ImGui (UI Library)
# -------------------------
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG docking
)
FetchContent_MakeAvailable(imgui)

# -------------------------
# nlohmann::json (Optional config/settings)
# -------------------------
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json
    GIT_TAG v3.12.0
)
FetchContent_MakeAvailable(nlohmann_json)

# -------------------------
# OpenGL (Rendering)
# -------------------------
find_package(OpenGL REQUIRED)

# ========================================================================
# Generated config header (project name, version, etc.)
# ========================================================================
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated/Core/Runtime")
configure_file(
    "${CMAKE_SOURCE_DIR}/src/Framework/Core/Runtime/AppConfig.h.in"
    "${CMAKE_BINARY_DIR}/generated/Core/Runtime/AppConfig.h"
    @ONLY
)

# ========================================================================
# ImGui Static Library Setup
# ========================================================================
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
    ${glfw_SOURCE_DIR}/include
)

# ========================================================================
# Framework Target (generic reusable infrastructure)
# ========================================================================
file(GLOB_RECURSE FRAMEWORK_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/Framework/*.cpp"
)

add_library(SoftwareFramework STATIC ${FRAMEWORK_SOURCES})

target_include_directories(SoftwareFramework PUBLIC
    "${CMAKE_SOURCE_DIR}/src/Framework"
    "${CMAKE_BINARY_DIR}/generated"
)

target_link_libraries(SoftwareFramework PUBLIC
    glfw
    imgui
    OpenGL::GL
    nlohmann_json
    ${CMAKE_DL_LIBS}
)

# ========================================================================
# Application Target (your project code built on top of the framework)
# ========================================================================
file(GLOB_RECURSE APP_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/App/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/Features/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/Modes/*.cpp"
)

add_executable(${PROJECT_NAME} ${APP_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    SoftwareFramework
)

# ========================================================================
# Tests
# ========================================================================
enable_testing()

file(GLOB_RECURSE SLATE_CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/App/Slate/*.cpp"
)
list(FILTER SLATE_CORE_SOURCES EXCLUDE REGEX ".*/UI/.*")

add_executable(SlateCoreTests
    tests/SlateCoreTests.cpp
    ${SLATE_CORE_SOURCES}
)

target_include_directories(SlateCoreTests PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    ${imgui_SOURCE_DIR}
)

add_test(NAME SlateCoreTests COMMAND $<TARGET_FILE:SlateCoreTests>)

# ========================================================================
# Platform-Specific Linking
# ========================================================================
if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE opengl32)
elseif(APPLE)
    find_library(COCOA_FRAMEWORK Cocoa)
    find_library(OPENGL_FRAMEWORK OpenGL)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${COCOA_FRAMEWORK} ${OPENGL_FRAMEWORK})
endif()
