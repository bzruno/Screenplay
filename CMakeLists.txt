cmake_minimum_required(VERSION 3.20)

# ── vcpkg integration (must come BEFORE project()) ───────────────────────────
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "vcpkg toolchain")
endif()

# ── Help CMake find Qt6 installed via vcpkg ──────────────────────────────────
list(APPEND CMAKE_PREFIX_PATH "C:/vcpkg/installed/x64-windows")

project(ScreenplayEditor VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets PrintSupport)
find_package(Freetype REQUIRED)

set(SOURCES
    src/main.cpp
    src/layout/freetype_metrics.cpp
    src/layout/layout_engine.cpp
)

add_executable(ScreenplayEditor ${SOURCES})

target_include_directories(ScreenplayEditor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${FREETYPE_INCLUDE_DIRS}
)

target_link_libraries(ScreenplayEditor PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::PrintSupport
    Freetype::Freetype
)

if(WIN32)
    set_target_properties(ScreenplayEditor PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

if(MSVC)
    target_compile_options(ScreenplayEditor PRIVATE /W4)
endif()