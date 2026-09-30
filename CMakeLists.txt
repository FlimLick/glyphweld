cmake_minimum_required(VERSION 3.19)

project(Glyphweld VERSION 0.1.0 LANGUAGES C)

include(CTest)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
  message(STATUS "Glyphweld size release: MinSizeRel, section GC, stripped executables")
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-ffunction-sections -fdata-sections)
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
      add_link_options(-Wl,--gc-sections -Wl,-s)
    endif()
    include(CheckIPOSupported)
    check_ipo_supported(RESULT GW_IPO_SUPPORTED OUTPUT GW_IPO_ERROR LANGUAGES C)
    if(GW_IPO_SUPPORTED)
      set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_MINSIZEREL ON)
      message(STATUS "Glyphweld size release: LTO enabled")
    else()
      message(STATUS "Glyphweld size release: LTO unavailable (${GW_IPO_ERROR})")
    endif()
  endif()
endif()

option(GW_REQUIRE_NINJA "Require the Ninja generator for this developer build" ON)
option(GLYPHWELD_BUILD_X11_HOST "Build the optional Linux X11/XCB host" OFF)
option(GLYPHWELD_BUILD_WAYLAND_HOST "Build the optional Linux Wayland host" OFF)
option(GLYPHWELD_BUILD_LINUX_DESKTOP_HOSTS
  "Auto-detect and build every available optional Linux desktop host" OFF)
set(GW_TARGET "headless" CACHE STRING "Glyphweld target pack")
set_property(CACHE GW_TARGET PROPERTY STRINGS headless)

if(NOT GW_TARGET STREQUAL "headless")
  message(FATAL_ERROR "Unknown target '${GW_TARGET}'. Milestone one provides only 'headless'.")
endif()

include(cmake/ResolveTargetToolchain.cmake)
gw_resolve_target_toolchain("${CMAKE_CURRENT_SOURCE_DIR}/targets/${GW_TARGET}/target.json")

set(GW_BUILD_X11_HOST ${GLYPHWELD_BUILD_X11_HOST})
set(GW_BUILD_WAYLAND_HOST ${GLYPHWELD_BUILD_WAYLAND_HOST})
if(GLYPHWELD_BUILD_LINUX_DESKTOP_HOSTS)
  set(GW_BUILD_X11_HOST ON)
  set(GW_BUILD_WAYLAND_HOST ON)
  message(STATUS "Glyphweld Linux desktop hosts: auto-detect enabled")
endif()

set(GW_DEFAULT_FONT_FILE "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/DejaVuSansMono.ttf")
set(GW_DEFAULT_FONT_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/generated/runtime/gw_default_font.c")
add_custom_command(OUTPUT "${GW_DEFAULT_FONT_SOURCE}"
  COMMAND ${CMAKE_COMMAND} "-DINPUT=${GW_DEFAULT_FONT_FILE}"
          "-DOUTPUT=${GW_DEFAULT_FONT_SOURCE}" -DSYMBOL=gw_default_ttf
          -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedBinary.cmake"
  DEPENDS "${GW_DEFAULT_FONT_FILE}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EmbedBinary.cmake"
  VERBATIM)

add_library(glyphweld_runtime STATIC
  src/runtime/src/core/runtime.c
  src/runtime/src/ui/tree.c
  src/runtime/src/ui/events.c
  src/runtime/src/layout/flex.c
  src/runtime/src/render/commands.c
  src/runtime/src/render/software.c
  "${GW_DEFAULT_FONT_SOURCE}"
)
target_include_directories(glyphweld_runtime PUBLIC src/runtime/include PRIVATE third_party/stb)
target_compile_options(glyphweld_runtime PRIVATE -Wall -Wextra -Werror -pedantic)
if(UNIX)
  target_link_libraries(glyphweld_runtime PUBLIC m)
endif()

add_library(glyphweld_target_headless STATIC targets/headless/headless_host.c)
target_include_directories(glyphweld_target_headless PUBLIC src/runtime/include targets/headless/include)
target_link_libraries(glyphweld_target_headless PUBLIC glyphweld_runtime)
target_compile_options(glyphweld_target_headless PRIVATE -Wall -Wextra -Werror -pedantic)

add_library(glyphweld_ui_compiler STATIC
  src/compiler/src/uic_diagnostic.c
  src/compiler/src/uic_json.c
  src/compiler/src/uic_model.c
  src/compiler/src/uic_emit.c
)
target_include_directories(glyphweld_ui_compiler PUBLIC src/compiler/include)
target_compile_options(glyphweld_ui_compiler PRIVATE -Wall -Wextra -Werror -pedantic)

add_executable(glyphweld_ui_compiler_tests src/compiler/tests/test_ui_compiler.c)
target_link_libraries(glyphweld_ui_compiler_tests PRIVATE glyphweld_ui_compiler)
target_compile_options(glyphweld_ui_compiler_tests PRIVATE -Wall -Wextra -Werror -pedantic)
add_test(NAME glyphweld_ui_compiler_tests COMMAND glyphweld_ui_compiler_tests)

add_executable(glyphweld_uic_driver src/tools/uic_driver.c)
target_link_libraries(glyphweld_uic_driver PRIVATE glyphweld_ui_compiler)
target_compile_options(glyphweld_uic_driver PRIVATE -Wall -Wextra -Werror -pedantic)
add_test(NAME glyphweld_uic_driver_transaction
  COMMAND ${CMAKE_COMMAND} -DDRIVER=$<TARGET_FILE:glyphweld_uic_driver>
          -DPROJECT=${CMAKE_CURRENT_SOURCE_DIR}/examples/counter/glyphweld.project.json
          -DWORK=${CMAKE_CURRENT_BINARY_DIR}/driver-transaction
          -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/TestDriverTransaction.cmake)
function(gw_add_generated_ui application project_file output_directory target_name)
  set(generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated/${application}")
  set(generated_header "${generated_dir}/${application}_ui.h")
  set(generated_source "${generated_dir}/${application}_ui.c")
  add_custom_command(OUTPUT "${generated_header}" "${generated_source}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${generated_dir}"
    COMMAND glyphweld_uic_driver "${project_file}" "${generated_header}" "${generated_source}"
    DEPENDS glyphweld_uic_driver "${project_file}" VERBATIM)
  add_library(${target_name} STATIC "${generated_source}")
  target_include_directories(${target_name} PUBLIC src/runtime/include "${generated_dir}")
  target_compile_options(${target_name} PRIVATE -Wall -Wextra -Werror -pedantic)
  set(${output_directory} "${generated_dir}" PARENT_SCOPE)
endfunction()

gw_add_generated_ui(counter "${CMAKE_CURRENT_SOURCE_DIR}/examples/counter/glyphweld.project.json"
  GW_COUNTER_GENERATED_DIR glyphweld_counter_ui)

add_executable(glyphweld_counter examples/counter/main.c examples/counter/app.c)
target_include_directories(glyphweld_counter PRIVATE src/runtime/include targets/headless/include examples/counter "${GW_COUNTER_GENERATED_DIR}")
target_link_libraries(glyphweld_counter PRIVATE glyphweld_target_headless glyphweld_counter_ui)
target_compile_options(glyphweld_counter PRIVATE -Wall -Wextra -Werror -pedantic)

add_executable(glyphweld_tests
  tests/test_main.c
  tests/test_layout.c
  tests/test_render.c
  tests/test_counter.c
  tests/test_runtime.c
  examples/counter/app.c
)
target_include_directories(glyphweld_tests PRIVATE src/runtime/include targets/headless/include examples/counter tests "${GW_COUNTER_GENERATED_DIR}")
target_link_libraries(glyphweld_tests PRIVATE glyphweld_target_headless glyphweld_counter_ui)
target_compile_options(glyphweld_tests PRIVATE -Wall -Wextra -Werror -pedantic)
add_test(NAME glyphweld_tests COMMAND glyphweld_tests)
add_test(NAME portable_runtime_guard_rejects_forbidden
  COMMAND ${CMAKE_COMMAND} -DGUARD_SCRIPT=${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckPortableRuntime.cmake
          -DWORK_ROOT=${CMAKE_CURRENT_BINARY_DIR}/guard-fixture
          -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/TestPortableRuntimeGuard.cmake)

add_custom_target(check-portable-runtime
  COMMAND ${CMAKE_COMMAND} -DGW_RUNTIME_ROOT=${CMAKE_CURRENT_SOURCE_DIR}/src/runtime
          -P ${CMAKE_CURRENT_SOURCE_DIR}/cmake/CheckPortableRuntime.cmake
  COMMENT "Checking portable runtime source restrictions")

if(GW_BUILD_X11_HOST)
  find_package(PkgConfig QUIET)
  if(NOT PkgConfig_FOUND)
    if(GLYPHWELD_BUILD_X11_HOST)
      message(FATAL_ERROR
        "GLYPHWELD_BUILD_X11_HOST=ON requires pkg-config and XCB development files. "
        "Install the XCB development package or configure with -DGLYPHWELD_BUILD_X11_HOST=OFF.")
    endif()
    message(STATUS "Glyphweld X11 host: disabled (pkg-config unavailable)")
  endif()
  if(PkgConfig_FOUND)
    pkg_check_modules(GW_XCB QUIET IMPORTED_TARGET xcb)
  endif()
  if(NOT GW_XCB_FOUND)
    if(GLYPHWELD_BUILD_X11_HOST)
      message(FATAL_ERROR
        "GLYPHWELD_BUILD_X11_HOST=ON requires XCB development files discoverable by pkg-config. "
        "Install the XCB development package or configure with -DGLYPHWELD_BUILD_X11_HOST=OFF.")
    endif()
    message(STATUS "Glyphweld X11 host: disabled (XCB development files unavailable)")
  else()
    message(STATUS "Glyphweld X11 host: enabled with XCB ${GW_XCB_VERSION}")
  add_library(glyphweld_target_linux_x11 STATIC targets/linux-x11/x11_host.c)
  target_include_directories(glyphweld_target_linux_x11 PUBLIC src/runtime/include targets/linux-x11/include)
  target_link_libraries(glyphweld_target_linux_x11 PUBLIC glyphweld_runtime PkgConfig::GW_XCB)
  target_compile_options(glyphweld_target_linux_x11 PRIVATE -Wall -Wextra -Werror -pedantic)

  add_executable(glyphweld_counter_x11 examples/counter/main_x11.c examples/counter/app.c)
  target_include_directories(glyphweld_counter_x11 PRIVATE src/runtime/include targets/linux-x11/include
    examples/counter "${GW_COUNTER_GENERATED_DIR}")
  target_link_libraries(glyphweld_counter_x11 PRIVATE glyphweld_target_linux_x11 glyphweld_counter_ui)
  target_compile_options(glyphweld_counter_x11 PRIVATE -Wall -Wextra -Werror -pedantic)

  if(NOT TARGET glyphweld_builder_ui)
    gw_add_generated_ui(builder "${CMAKE_CURRENT_SOURCE_DIR}/examples/builder/glyphweld.project.json"
      GW_BUILDER_GENERATED_DIR glyphweld_builder_ui)
    gw_add_generated_ui(widget_gallery
      "${CMAKE_CURRENT_SOURCE_DIR}/examples/widget-gallery/glyphweld.project.json"
      GW_WIDGET_GALLERY_GENERATED_DIR glyphweld_widget_gallery_ui)
  endif()
  add_executable(glyphweld_builder_x11 examples/builder/main_x11.c examples/builder/app.c)
  target_include_directories(glyphweld_builder_x11 PRIVATE src/runtime/include targets/linux-x11/include
    examples/builder "${GW_BUILDER_GENERATED_DIR}")
  target_link_libraries(glyphweld_builder_x11 PRIVATE glyphweld_target_linux_x11 glyphweld_builder_ui)
  target_compile_options(glyphweld_builder_x11 PRIVATE -Wall -Wextra -Werror -pedantic)

  add_executable(glyphweld_widget_gallery_x11 examples/widget-gallery/main_x11.c
    examples/widget-gallery/app.c)
  target_include_directories(glyphweld_widget_gallery_x11 PRIVATE src/runtime/include
    targets/linux-x11/include examples/widget-gallery "${GW_WIDGET_GALLERY_GENERATED_DIR}")
  target_link_libraries(glyphweld_widget_gallery_x11 PRIVATE glyphweld_target_linux_x11
    glyphweld_widget_gallery_ui)
  target_compile_options(glyphweld_widget_gallery_x11 PRIVATE -Wall -Wextra -Werror -pedantic)

  add_executable(glyphweld_x11_host_tests tests/test_x11_main.c tests/test_x11_host.c)
  target_include_directories(glyphweld_x11_host_tests PRIVATE src/runtime/include targets/linux-x11/include tests)
  target_link_libraries(glyphweld_x11_host_tests PRIVATE glyphweld_target_linux_x11)
  target_compile_options(glyphweld_x11_host_tests PRIVATE -Wall -Wextra -Werror -pedantic)
  add_test(NAME glyphweld_x11_host_tests COMMAND glyphweld_x11_host_tests)
  endif()
endif()

if(GW_BUILD_WAYLAND_HOST)
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(GW_WAYLAND_CLIENT QUIET IMPORTED_TARGET wayland-client)
    pkg_check_modules(GW_WAYLAND_CURSOR QUIET IMPORTED_TARGET wayland-cursor)
    pkg_check_modules(GW_XKBCOMMON QUIET IMPORTED_TARGET xkbcommon)
  endif()
  find_program(GW_WAYLAND_SCANNER wayland-scanner)
  set(GW_XDG_SHELL_XML "/usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml")
  if(NOT EXISTS "${GW_XDG_SHELL_XML}")
    find_file(GW_XDG_SHELL_XML xdg-shell.xml
      PATHS /usr/share/wayland-protocols/stable/xdg-shell)
  endif()
  if(NOT PkgConfig_FOUND OR NOT GW_WAYLAND_CLIENT_FOUND OR NOT GW_WAYLAND_CURSOR_FOUND OR
     NOT GW_XKBCOMMON_FOUND OR
     NOT GW_WAYLAND_SCANNER OR NOT EXISTS "${GW_XDG_SHELL_XML}")
    if(GLYPHWELD_BUILD_WAYLAND_HOST)
      message(FATAL_ERROR
        "GLYPHWELD_BUILD_WAYLAND_HOST=ON requires pkg-config, wayland-client, wayland-cursor, xkbcommon, "
        "wayland-scanner, and xdg-shell.xml from wayland-protocols. Install the Wayland development "
        "packages or configure with -DGLYPHWELD_BUILD_WAYLAND_HOST=OFF.")
    endif()
    message(STATUS "Glyphweld Wayland host: disabled (development prerequisites unavailable)")
  else()
    set(GW_WAYLAND_PROTOCOL_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated/wayland")
    set(GW_XDG_SHELL_HEADER "${GW_WAYLAND_PROTOCOL_DIR}/xdg-shell-client-protocol.h")
    set(GW_XDG_SHELL_SOURCE "${GW_WAYLAND_PROTOCOL_DIR}/xdg-shell-protocol.c")
    add_custom_command(OUTPUT "${GW_XDG_SHELL_HEADER}" "${GW_XDG_SHELL_SOURCE}"
      COMMAND ${CMAKE_COMMAND} -E make_directory "${GW_WAYLAND_PROTOCOL_DIR}"
      COMMAND "${GW_WAYLAND_SCANNER}" client-header "${GW_XDG_SHELL_XML}" "${GW_XDG_SHELL_HEADER}"
      COMMAND "${GW_WAYLAND_SCANNER}" private-code "${GW_XDG_SHELL_XML}" "${GW_XDG_SHELL_SOURCE}"
      DEPENDS "${GW_XDG_SHELL_XML}"
      VERBATIM)
    set(GW_WAYLAND_PROTOCOL_SOURCES "${GW_XDG_SHELL_SOURCE}")
    set(GW_XDG_DECORATION_XML
      "/usr/share/wayland-protocols/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml")
    if(EXISTS "${GW_XDG_DECORATION_XML}")
      set(GW_XDG_DECORATION_HEADER
        "${GW_WAYLAND_PROTOCOL_DIR}/xdg-decoration-client-protocol.h")
      set(GW_XDG_DECORATION_SOURCE "${GW_WAYLAND_PROTOCOL_DIR}/xdg-decoration-protocol.c")
      add_custom_command(OUTPUT "${GW_XDG_DECORATION_HEADER}" "${GW_XDG_DECORATION_SOURCE}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${GW_WAYLAND_PROTOCOL_DIR}"
        COMMAND "${GW_WAYLAND_SCANNER}" client-header "${GW_XDG_DECORATION_XML}"
                "${GW_XDG_DECORATION_HEADER}"
        COMMAND "${GW_WAYLAND_SCANNER}" private-code "${GW_XDG_DECORATION_XML}"
                "${GW_XDG_DECORATION_SOURCE}"
        DEPENDS "${GW_XDG_DECORATION_XML}"
        VERBATIM)
      list(APPEND GW_WAYLAND_PROTOCOL_SOURCES "${GW_XDG_DECORATION_SOURCE}")
      set(GW_WAYLAND_HAVE_XDG_DECORATION ON)
    endif()
    set(GW_CURSOR_SHAPE_XML
      "/usr/share/wayland-protocols/staging/cursor-shape/cursor-shape-v1.xml")
    set(GW_TABLET_V2_XML "/usr/share/wayland-protocols/stable/tablet/tablet-v2.xml")
    if(EXISTS "${GW_CURSOR_SHAPE_XML}" AND EXISTS "${GW_TABLET_V2_XML}")
      set(GW_CURSOR_SHAPE_HEADER "${GW_WAYLAND_PROTOCOL_DIR}/cursor-shape-client-protocol.h")
      set(GW_CURSOR_SHAPE_SOURCE "${GW_WAYLAND_PROTOCOL_DIR}/cursor-shape-protocol.c")
      add_custom_command(OUTPUT "${GW_CURSOR_SHAPE_HEADER}" "${GW_CURSOR_SHAPE_SOURCE}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${GW_WAYLAND_PROTOCOL_DIR}"
        COMMAND "${GW_WAYLAND_SCANNER}" client-header "${GW_CURSOR_SHAPE_XML}"
                "${GW_CURSOR_SHAPE_HEADER}"
        COMMAND "${GW_WAYLAND_SCANNER}" private-code "${GW_CURSOR_SHAPE_XML}"
                "${GW_CURSOR_SHAPE_SOURCE}"
        DEPENDS "${GW_CURSOR_SHAPE_XML}"
        VERBATIM)
      set(GW_TABLET_V2_SOURCE "${GW_WAYLAND_PROTOCOL_DIR}/tablet-v2-protocol.c")
      add_custom_command(OUTPUT "${GW_TABLET_V2_SOURCE}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${GW_WAYLAND_PROTOCOL_DIR}"
        COMMAND "${GW_WAYLAND_SCANNER}" private-code "${GW_TABLET_V2_XML}"
                "${GW_TABLET_V2_SOURCE}"
        DEPENDS "${GW_TABLET_V2_XML}"
        VERBATIM)
      list(APPEND GW_WAYLAND_PROTOCOL_SOURCES "${GW_CURSOR_SHAPE_SOURCE}"
           "${GW_TABLET_V2_SOURCE}")
      set(GW_WAYLAND_HAVE_CURSOR_SHAPE ON)
    endif()
    add_library(glyphweld_wayland_protocol STATIC ${GW_WAYLAND_PROTOCOL_SOURCES})
    target_include_directories(glyphweld_wayland_protocol PUBLIC "${GW_WAYLAND_PROTOCOL_DIR}")
    target_link_libraries(glyphweld_wayland_protocol PUBLIC PkgConfig::GW_WAYLAND_CLIENT)

    message(STATUS "Glyphweld Wayland host: enabled with Wayland ${GW_WAYLAND_CLIENT_VERSION}")
    add_library(glyphweld_target_linux_wayland STATIC targets/linux-wayland/wayland_host.c)
    target_include_directories(glyphweld_target_linux_wayland PUBLIC src/runtime/include
      targets/linux-wayland/include "${GW_WAYLAND_PROTOCOL_DIR}")
    target_link_libraries(glyphweld_target_linux_wayland PUBLIC glyphweld_runtime
      glyphweld_wayland_protocol PkgConfig::GW_WAYLAND_CLIENT PkgConfig::GW_WAYLAND_CURSOR
      PkgConfig::GW_XKBCOMMON)
    target_compile_options(glyphweld_target_linux_wayland PRIVATE -Wall -Wextra -Werror -pedantic)
    if(GW_WAYLAND_HAVE_XDG_DECORATION)
      target_compile_definitions(glyphweld_target_linux_wayland PRIVATE
        GW_WAYLAND_HAVE_XDG_DECORATION=1)
      message(STATUS "Glyphweld Wayland host: requesting server-side decorations when supported")
    endif()
    if(GW_WAYLAND_HAVE_CURSOR_SHAPE)
      target_compile_definitions(glyphweld_target_linux_wayland PRIVATE
        GW_WAYLAND_HAVE_CURSOR_SHAPE=1)
      message(STATUS "Glyphweld Wayland host: using compositor cursor shapes when supported")
    endif()

    add_executable(glyphweld_counter_wayland examples/counter/main_wayland.c examples/counter/app.c)
    target_include_directories(glyphweld_counter_wayland PRIVATE src/runtime/include
      targets/linux-wayland/include examples/counter "${GW_COUNTER_GENERATED_DIR}")
    target_link_libraries(glyphweld_counter_wayland PRIVATE glyphweld_target_linux_wayland
      glyphweld_counter_ui)
    target_compile_options(glyphweld_counter_wayland PRIVATE -Wall -Wextra -Werror -pedantic)

    if(NOT TARGET glyphweld_builder_ui)
      gw_add_generated_ui(builder "${CMAKE_CURRENT_SOURCE_DIR}/examples/builder/glyphweld.project.json"
        GW_BUILDER_GENERATED_DIR glyphweld_builder_ui)
      gw_add_generated_ui(widget_gallery
        "${CMAKE_CURRENT_SOURCE_DIR}/examples/widget-gallery/glyphweld.project.json"
        GW_WIDGET_GALLERY_GENERATED_DIR glyphweld_widget_gallery_ui)
    endif()
    add_executable(glyphweld_builder_wayland examples/builder/main_wayland.c examples/builder/app.c)
    target_include_directories(glyphweld_builder_wayland PRIVATE src/runtime/include
      targets/linux-wayland/include examples/builder "${GW_BUILDER_GENERATED_DIR}")
    target_link_libraries(glyphweld_builder_wayland PRIVATE glyphweld_target_linux_wayland
      glyphweld_builder_ui)
    target_compile_options(glyphweld_builder_wayland PRIVATE -Wall -Wextra -Werror -pedantic)

    add_executable(glyphweld_widget_gallery_wayland examples/widget-gallery/main_wayland.c
      examples/widget-gallery/app.c)
    target_include_directories(glyphweld_widget_gallery_wayland PRIVATE src/runtime/include
      targets/linux-wayland/include examples/widget-gallery "${GW_WIDGET_GALLERY_GENERATED_DIR}")
    target_link_libraries(glyphweld_widget_gallery_wayland PRIVATE glyphweld_target_linux_wayland
      glyphweld_widget_gallery_ui)
    target_compile_options(glyphweld_widget_gallery_wayland PRIVATE -Wall -Wextra -Werror -pedantic)

    add_executable(glyphweld_wayland_host_tests tests/test_wayland_main.c tests/test_wayland_host.c)
    target_include_directories(glyphweld_wayland_host_tests PRIVATE src/runtime/include
      targets/linux-wayland/include tests)
    target_link_libraries(glyphweld_wayland_host_tests PRIVATE glyphweld_target_linux_wayland)
    target_compile_options(glyphweld_wayland_host_tests PRIVATE -Wall -Wextra -Werror -pedantic)
    add_test(NAME glyphweld_wayland_host_tests COMMAND glyphweld_wayland_host_tests)
  endif()
endif()
