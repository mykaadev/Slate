cmake_minimum_required(VERSION 3.15)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(AppMetadata)
project(${SOFTWARE_TARGET_NAME} VERSION ${SOFTWARE_APP_VERSION} LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(OpenGL_GL_PREFERENCE "LEGACY")
include(FetchContent)
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
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG docking
)
FetchContent_MakeAvailable(imgui)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json
    GIT_TAG v3.12.0
)
FetchContent_MakeAvailable(nlohmann_json)

# Lua powers Slate plugins. It is compiled as C++ so script errors unwind
# through C++ frames with exceptions instead of longjmp.
FetchContent_Declare(
    lua_src
    URL https://www.lua.org/ftp/lua-5.4.7.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(lua_src)
file(GLOB LUA_CORE_SOURCES CONFIGURE_DEPENDS "${lua_src_SOURCE_DIR}/src/*.c")
list(FILTER LUA_CORE_SOURCES EXCLUDE REGEX ".*/(lua|luac)\\.c$")
set_source_files_properties(${LUA_CORE_SOURCES} PROPERTIES LANGUAGE CXX)
add_library(SlateLua STATIC ${LUA_CORE_SOURCES})
target_include_directories(SlateLua PUBLIC "${lua_src_SOURCE_DIR}/src")
if(MSVC)
    target_compile_options(SlateLua PRIVATE /TP /EHsc /wd4244 /wd4324 /wd4702)
    target_compile_definitions(SlateLua PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

find_package(OpenGL REQUIRED)
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated/Core/Runtime")
configure_file(
    "${CMAKE_SOURCE_DIR}/src/Framework/Core/Runtime/AppConfig.h.in"
    "${CMAKE_BINARY_DIR}/generated/Core/Runtime/AppConfig.h"
    @ONLY
)
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
file(GLOB_RECURSE SLATE_APP_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/App/Application.cpp"
    "${CMAKE_SOURCE_DIR}/src/App/DefaultModules/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/App/Runtime/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/App/Slate/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/Modes/Slate/*.cpp"
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
    list(APPEND SLATE_APP_SOURCES "${CMAKE_BINARY_DIR}/generated/App/Resources/WindowsApp.rc")
elseif(WIN32)
    message(STATUS "No Windows app icon found at ${SLATE_APP_ICON_PATH}; building without an embedded icon.")
endif()

if(WIN32)
    add_executable(${PROJECT_NAME} WIN32 ${SLATE_APP_SOURCES})
else()
    add_executable(${PROJECT_NAME} ${SLATE_APP_SOURCES})
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    SoftwareFramework
    SlateLua
)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        gdiplus
    )
endif()

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory
            "${CMAKE_SOURCE_DIR}/assets/slate"
            "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets/slate"
    COMMENT "Copying Slate preset assets"
)
enable_testing()

file(GLOB_RECURSE SLATE_CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/App/Slate/*.cpp"
)
list(FILTER SLATE_CORE_SOURCES EXCLUDE REGEX ".*/Overlays/.*")
list(FILTER SLATE_CORE_SOURCES EXCLUDE REGEX ".*/Modules/.*")
list(FILTER SLATE_CORE_SOURCES EXCLUDE REGEX ".*/Commands/.*")

add_executable(SlateCoreTests
    tests/SlateCoreTests.cpp
    ${SLATE_CORE_SOURCES}
)

target_include_directories(SlateCoreTests PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/src/Framework"
    "${CMAKE_BINARY_DIR}/generated"
    ${imgui_SOURCE_DIR}
)

target_link_libraries(SlateCoreTests PRIVATE
    SoftwareFramework
    SlateLua
)

add_test(NAME SlateCoreTests COMMAND $<TARGET_FILE:SlateCoreTests>)

if(WIN32)
    target_link_libraries(SlateCoreTests PRIVATE
        gdiplus
    )
endif()
if(WIN32)
    target_link_libraries(SoftwareFramework PRIVATE dwmapi)
    target_link_libraries(${PROJECT_NAME} PRIVATE opengl32)
elseif(APPLE)
    find_library(COCOA_FRAMEWORK Cocoa)
    find_library(OPENGL_FRAMEWORK OpenGL)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${COCOA_FRAMEWORK} ${OPENGL_FRAMEWORK})
endif()
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION "."
)

install(FILES
    "${CMAKE_SOURCE_DIR}/README.md"
    DESTINATION "docs"
)

install(DIRECTORY
    "${CMAKE_SOURCE_DIR}/resources"
    DESTINATION "docs"
)

install(DIRECTORY
    "${CMAKE_SOURCE_DIR}/assets/slate"
    DESTINATION "assets"
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
