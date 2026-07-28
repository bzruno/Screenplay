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

# ── Optional: SVG cover artwork ──────────────────────────────────────────────
# A vector logo is the one a production actually has, so SVG is worth reading.
# It needs Qt's SVG module, which is a separate package: without it Qt cannot
# decode SVG at all and the cover's file picker will not offer it.
#   vcpkg install qtsvg:x64-windows
# Optional on purpose — the app must still build on a machine that lacks it.
find_package(Qt6 QUIET COMPONENTS Svg)

# Headers declaring Q_OBJECT must be listed so AUTOMOC processes them.
set(SOURCES
    src/main.cpp
    src/screenplay_canvas.hpp
    src/main_window.hpp
    src/ui/toast.hpp
    src/ui/script_language_dialog.hpp
    src/ui/autocomplete_popup.hpp
    src/ui/search_bar.hpp
    src/ui/script_panel.hpp
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

if(Qt6Svg_FOUND)
    target_link_libraries(ScreenplayEditor PRIVATE Qt6::Svg)
    message(STATUS "SVG cover artwork: enabled")
else()
    message(STATUS "SVG cover artwork: DISABLED "
                   "(run: vcpkg install qtsvg:x64-windows)")
endif()

# ── Deploy Qt ─────────────────────────────────────────────────────────────────
# An image-format plugin is a DLL WITH ITS OWN DEPENDENCIES: qsvg.dll needs
# Qt6Svg.dll, qjpeg.dll needs the JPEG runtime. Copying the plugin folder alone
# looks right and silently does nothing — Qt tries to load the plugin, cannot
# resolve its imports, and drops the format without a word. That is exactly why
# "SVG: enabled" appeared in the build while the app still would not open an
# .svg, and why JPEG was missing too.
#
# windeployqt is the tool that walks those dependencies, so it does the job
# instead of a hand-maintained copy list.
get_target_property(_qt_core_location Qt6::Core LOCATION)
get_filename_component(_qt_bin_dir "${_qt_core_location}" DIRECTORY)
find_program(WINDEPLOYQT windeployqt
             HINTS "${_qt_bin_dir}" "${_qt_bin_dir}/../tools/Qt6/bin")
if(WINDEPLOYQT)
    add_custom_command(TARGET ScreenplayEditor POST_BUILD
        COMMAND "${WINDEPLOYQT}" --no-translations --no-compiler-runtime
                --no-system-d3d-compiler --verbose 0
                "$<TARGET_FILE:ScreenplayEditor>"
        COMMENT "Deploying Qt runtime and plugins (windeployqt)")
else()
    message(WARNING "windeployqt not found — image formats that need a plugin "
                    "(JPEG, SVG) will not load at runtime")
endif()

# windeployqt walks Qt's OWN dependencies and stops there. An image plugin can
# also depend on a third-party codec — qjpeg.dll needs jpeg62.dll — and without
# it the plugin fails to load exactly as silently as before: JPEG simply went
# missing from the file dialog. Copied when present, ignored when not, so this
# stays correct if a codec is ever linked statically instead.
foreach(_codec jpeg62 turbojpeg tiff webp webpdemux)
    if(EXISTS "${_qt_bin_dir}/${_codec}.dll")
        add_custom_command(TARGET ScreenplayEditor POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${_qt_bin_dir}/${_codec}.dll"
                    "$<TARGET_FILE_DIR:ScreenplayEditor>"
            COMMENT "Deploying image codec ${_codec}")
    endif()
endforeach()

if(WIN32)
    set_target_properties(ScreenplayEditor PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# ── Bundle Courier Prime next to the executable ──────────────────────────────
# The layout engine measures with Courier Prime and the PDF export embeds it, so
# the font must ship with the app rather than depending on a system install.
# Place CourierPrime-{Regular,Bold,Italic,BoldItalic}.ttf in ./fonts; this copies
# them beside the binary on every build.
add_custom_command(TARGET ScreenplayEditor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/fonts"
            "$<TARGET_FILE_DIR:ScreenplayEditor>/fonts"
    COMMENT "Bundling Courier Prime fonts next to the executable")

if(MSVC)
    target_compile_options(ScreenplayEditor PRIVATE /W4)
endif()