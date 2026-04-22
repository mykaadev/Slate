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

if(MSVC)
    # Static runtime keeps the portable build down to the app itself.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

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
set(GLFW_BUILD_DOCS OFF CACHE BOOL "Disable GLFW docs" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "Disable GLFW examples" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "Disable GLFW tests" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "Disable GLFW install rules" FORCE)
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

if(WIN32)
    FetchContent_Declare(
        scintilla_src
        URL https://www.scintilla.org/scintilla561.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(scintilla_src)

    FetchContent_Declare(
        lexilla_src
        URL https://www.scintilla.org/lexilla546.zip
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(lexilla_src)

    set(SCINTILLA_ROOT "${scintilla_src_SOURCE_DIR}")
    if(EXISTS "${scintilla_src_SOURCE_DIR}/scintilla")
        set(SCINTILLA_ROOT "${scintilla_src_SOURCE_DIR}/scintilla")
    endif()

    set(LEXILLA_ROOT "${lexilla_src_SOURCE_DIR}")
    if(EXISTS "${lexilla_src_SOURCE_DIR}/lexilla")
        set(LEXILLA_ROOT "${lexilla_src_SOURCE_DIR}/lexilla")
    endif()

    file(GLOB SCINTILLA_CORE_SOURCES CONFIGURE_DEPENDS
        "${SCINTILLA_ROOT}/src/*.cxx"
    )
    list(APPEND SCINTILLA_CORE_SOURCES
        "${SCINTILLA_ROOT}/win32/HanjaDic.cxx"
        "${SCINTILLA_ROOT}/win32/ListBox.cxx"
        "${SCINTILLA_ROOT}/win32/PlatWin.cxx"
        "${SCINTILLA_ROOT}/win32/ScintillaWin.cxx"
        "${SCINTILLA_ROOT}/win32/SurfaceD2D.cxx"
        "${SCINTILLA_ROOT}/win32/SurfaceGDI.cxx"
    )

    add_library(ScintillaCore STATIC ${SCINTILLA_CORE_SOURCES})
    target_include_directories(ScintillaCore PUBLIC
        "${SCINTILLA_ROOT}/include"
        "${SCINTILLA_ROOT}/src"
        "${SCINTILLA_ROOT}/win32"
    )
    target_compile_definitions(ScintillaCore PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_link_libraries(ScintillaCore PUBLIC
        user32
        gdi32
        imm32
        ole32
        oleaut32
        advapi32
    )

    file(GLOB LEXILLA_SUPPORT_SOURCES CONFIGURE_DEPENDS
        "${LEXILLA_ROOT}/lexlib/*.cxx"
    )
    add_library(SlateMarkdownLexer STATIC
        ${LEXILLA_SUPPORT_SOURCES}
        "${LEXILLA_ROOT}/lexers/LexMarkdown.cxx"
    )
    target_include_directories(SlateMarkdownLexer PUBLIC
        "${LEXILLA_ROOT}/include"
        "${LEXILLA_ROOT}/lexlib"
        "${SCINTILLA_ROOT}/include"
    )
    target_compile_definitions(SlateMarkdownLexer PRIVATE _CRT_SECURE_NO_WARNINGS _CRT_SECURE_NO_DEPRECATE=1)
endif()

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

set(SLATE_APP_ICON_PATH "${CMAKE_SOURCE_DIR}/assets/app/icon.ico")
if(WIN32 AND EXISTS "${SLATE_APP_ICON_PATH}")
    file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated/App/Resources")
    set(SLATE_APP_ICON_PATH_RC "${SLATE_APP_ICON_PATH}")
    configure_file(
        "${CMAKE_SOURCE_DIR}/src/App/Resources/WindowsApp.rc.in"
        "${CMAKE_BINARY_DIR}/generated/App/Resources/WindowsApp.rc"
        @ONLY
    )
    list(APPEND APP_SOURCES "${CMAKE_BINARY_DIR}/generated/App/Resources/WindowsApp.rc")
elseif(WIN32)
    message(STATUS "No Windows app icon found at ${SLATE_APP_ICON_PATH}; building without an embedded icon.")
endif()

if(WIN32)
    add_executable(${PROJECT_NAME} WIN32 ${APP_SOURCES})
else()
    add_executable(${PROJECT_NAME} ${APP_SOURCES})
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    SoftwareFramework
)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ScintillaCore
        SlateMarkdownLexer
    )
endif()

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

if(WIN32)
    target_link_libraries(SlateCoreTests PRIVATE
        ScintillaCore
        SlateMarkdownLexer
    )
endif()

# ========================================================================
# Platform-Specific Linking
# ========================================================================
if(WIN32)
    target_link_libraries(SoftwareFramework PRIVATE dwmapi)
    target_link_libraries(${PROJECT_NAME} PRIVATE opengl32)
elseif(APPLE)
    find_library(COCOA_FRAMEWORK Cocoa)
    find_library(OPENGL_FRAMEWORK OpenGL)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${COCOA_FRAMEWORK} ${OPENGL_FRAMEWORK})
endif()

# ========================================================================
# Portable Packaging
# ========================================================================
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION "."
)

install(FILES
    "${CMAKE_SOURCE_DIR}/README.md"
    DESTINATION "docs"
)

set(CPACK_GENERATOR "ZIP")
set(CPACK_OUTPUT_FILE_PREFIX "${CMAKE_BINARY_DIR}/portable")
set(CPACK_PACKAGE_FILE_NAME "${SOFTWARE_TARGET_NAME}-${SOFTWARE_APP_VERSION}-windows-portable")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
include(CPack)

add_custom_target(portable
    COMMAND "${CMAKE_CPACK_COMMAND}" --config "${CMAKE_BINARY_DIR}/CPackConfig.cmake" -C $<CONFIG>
    DEPENDS ${PROJECT_NAME}
    COMMENT "Building portable zip"
)
