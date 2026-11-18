cmake_minimum_required(VERSION 3.16)
project(bass_suite VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
include(GNUInstallDirs)

enable_testing()

set(CLAP_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/clap"
    CACHE PATH "Directory where CLAP bundles are installed")
set(LV2_DIR "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/lv2"
    CACHE PATH "Directory where the LV2 bundle is installed")

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)
pkg_check_modules(LV2 REQUIRED IMPORTED_TARGET lv2)
pkg_check_modules(CAIRO REQUIRED IMPORTED_TARGET cairo)
pkg_check_modules(FREETYPE REQUIRED IMPORTED_TARGET freetype2)
pkg_check_modules(X11 REQUIRED IMPORTED_TARGET x11)
pkg_check_modules(SAMPLERATE REQUIRED IMPORTED_TARGET samplerate)

file(GLOB BASS_NAM_SOURCES
  "${CMAKE_SOURCE_DIR}/third_party/nam/NAM/*.cpp"
  "${CMAKE_SOURCE_DIR}/third_party/nam/NAM/wavenet/*.cpp")
add_library(nam_core OBJECT ${BASS_NAM_SOURCES})
target_include_directories(nam_core PUBLIC
  "${CMAKE_SOURCE_DIR}/third_party/nam/NAM"
  "${CMAKE_SOURCE_DIR}/third_party/nam/Dependencies/nlohmann"
  "${CMAKE_SOURCE_DIR}/third_party/nam/Dependencies/eigen")
target_compile_features(nam_core PUBLIC cxx_std_20)
target_compile_definitions(nam_core PUBLIC NAM_SAMPLE_FLOAT)
set_target_properties(nam_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  # Two suites can be loaded into the same host.  NAM has process-global
  # registries; leaving its symbols visible lets one plug-in's copy interpose
  # on the other's, so the second dlopen sees "already registered" and aborts.
  target_compile_options(nam_core PRIVATE
    -fvisibility=hidden -fvisibility-inlines-hidden)
endif()

set(BASS_UI_SOURCES src/ui/panel.cpp)
set(BASS_DSP_SOURCES
  src/dsp/guitarix_tables.cpp
  src/dsp/power_amp.cpp
  src/dsp/ts9.cpp)
set(BASS_GUITARIX_INCLUDES
  "${CMAKE_SOURCE_DIR}/third_party/guitarix/DSP"
  "${CMAKE_SOURCE_DIR}/third_party/guitarix/DSP/tube_tables"
  "${CMAKE_SOURCE_DIR}/third_party/guitarix/plugins")

# ---------------------------------------------------------------------------
# CLAP plugin: four bass amps, one X11/Cairo editor.
# ---------------------------------------------------------------------------
add_library(bass_suite MODULE
  src/clap/bass_suite_clap.cpp
  ${BASS_DSP_SOURCES}
  ${BASS_UI_SOURCES})
target_include_directories(bass_suite PRIVATE
  "${CMAKE_SOURCE_DIR}/src"
  "${CMAKE_SOURCE_DIR}/third_party/clap/include"
  ${BASS_GUITARIX_INCLUDES})
target_link_libraries(bass_suite PRIVATE
  PkgConfig::CAIRO PkgConfig::FREETYPE PkgConfig::X11
  PkgConfig::SAMPLERATE nam_core
  Threads::Threads ${CMAKE_DL_LIBS})
target_compile_features(bass_suite PRIVATE cxx_std_20)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(bass_suite PRIVATE -Wall -Wextra -fvisibility=hidden)
endif()
set_target_properties(bass_suite PROPERTIES
  PREFIX ""
  OUTPUT_NAME "BassSuite"
  SUFFIX ".clap"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/clap")

set(BASS_RESOURCE_FILES
  "${CMAKE_SOURCE_DIR}/assets/fonts/Jost-ExtraBold.ttf"
  "${CMAKE_SOURCE_DIR}/assets/fonts/OFL.txt")
file(GLOB BASS_IR_FILES "${CMAKE_SOURCE_DIR}/assets/irs/*/*.wav")

add_custom_target(bass_suite_bundle ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/clap/BassSuite.resources"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different ${BASS_RESOURCE_FILES}
          "${CMAKE_BINARY_DIR}/clap/BassSuite.resources/"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_SOURCE_DIR}/assets/irs"
          "${CMAKE_BINARY_DIR}/clap/BassSuite.resources/irs"
  DEPENDS bass_suite ${BASS_RESOURCE_FILES} ${BASS_IR_FILES})

install(TARGETS bass_suite LIBRARY DESTINATION "${CLAP_DIR}")
install(FILES ${BASS_RESOURCE_FILES}
        DESTINATION "${CLAP_DIR}/BassSuite.resources")
install(DIRECTORY "${CMAKE_SOURCE_DIR}/assets/irs"
        DESTINATION "${CLAP_DIR}/BassSuite.resources")

# ---------------------------------------------------------------------------
# LV2 plugin and X11 UI
# ---------------------------------------------------------------------------
add_library(bass_suite_lv2 MODULE
  src/bass_suite_lv2.cpp
  ${BASS_DSP_SOURCES})
target_include_directories(bass_suite_lv2 PRIVATE
  "${CMAKE_SOURCE_DIR}/src"
  ${BASS_GUITARIX_INCLUDES})
target_link_libraries(bass_suite_lv2 PRIVATE PkgConfig::LV2 PkgConfig::SAMPLERATE nam_core Threads::Threads)
target_compile_features(bass_suite_lv2 PRIVATE cxx_std_20)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(bass_suite_lv2 PRIVATE -Wall -Wextra -fvisibility=hidden)
endif()
set_target_properties(bass_suite_lv2 PROPERTIES
  PREFIX ""
  OUTPUT_NAME "bass_suite"
  SUFFIX ".so"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lv2/bass-suite.lv2")

add_library(bass_suite_ui MODULE
  src/ui/lv2_ui.cpp
  ${BASS_UI_SOURCES})
target_include_directories(bass_suite_ui PRIVATE
  "${CMAKE_SOURCE_DIR}/src"
  "${CMAKE_SOURCE_DIR}/third_party/clap/include")
target_link_libraries(bass_suite_ui PRIVATE
  PkgConfig::LV2 PkgConfig::CAIRO PkgConfig::FREETYPE PkgConfig::X11
  Threads::Threads ${CMAKE_DL_LIBS})
target_compile_features(bass_suite_ui PRIVATE cxx_std_20)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(bass_suite_ui PRIVATE -Wall -Wextra -fvisibility=hidden)
endif()
set_target_properties(bass_suite_ui PROPERTIES
  PREFIX ""
  OUTPUT_NAME "bass_suite_ui"
  SUFFIX ".so"
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lv2/bass-suite.lv2")

# Metadata is generated from params.hpp/presets.hpp.
add_executable(bass_ttl_gen tools/gen_ttl.cpp)
target_include_directories(bass_ttl_gen PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_compile_features(bass_ttl_gen PRIVATE cxx_std_20)

add_custom_target(bass_lv2_ttl ALL
  COMMAND $<TARGET_FILE:bass_ttl_gen> --root "${CMAKE_SOURCE_DIR}"
  DEPENDS bass_ttl_gen)
add_custom_target(bass_lv2_bundle ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/lv2/bass-suite.lv2"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_SOURCE_DIR}/lv2/bass_suite.ttl"
          "${CMAKE_SOURCE_DIR}/lv2/manifest.ttl"
          "${CMAKE_SOURCE_DIR}/lv2/presets.ttl"
          "${CMAKE_BINARY_DIR}/lv2/bass-suite.lv2/"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          "${CMAKE_SOURCE_DIR}/assets/irs"
          "${CMAKE_BINARY_DIR}/lv2/bass-suite.lv2/irs"
  DEPENDS bass_suite_lv2 bass_suite_ui bass_lv2_ttl ${BASS_IR_FILES})

install(TARGETS bass_suite_lv2 bass_suite_ui
        LIBRARY DESTINATION "${LV2_DIR}/bass-suite.lv2")
install(FILES
          "${CMAKE_SOURCE_DIR}/lv2/bass_suite.ttl"
          "${CMAKE_SOURCE_DIR}/lv2/manifest.ttl"
          "${CMAKE_SOURCE_DIR}/lv2/presets.ttl"
        DESTINATION "${LV2_DIR}/bass-suite.lv2")
install(DIRECTORY "${CMAKE_SOURCE_DIR}/assets/irs"
        DESTINATION "${LV2_DIR}/bass-suite.lv2")

# ---------------------------------------------------------------------------
# Tests and tools
# ---------------------------------------------------------------------------
add_executable(bass_dsp_test test/test_dsp.cpp ${BASS_DSP_SOURCES})
target_include_directories(bass_dsp_test PRIVATE
  "${CMAKE_SOURCE_DIR}/src"
  ${BASS_GUITARIX_INCLUDES})
target_link_libraries(bass_dsp_test PRIVATE PkgConfig::SAMPLERATE nam_core Threads::Threads)
target_compile_definitions(bass_dsp_test PRIVATE
  BASS_TEST_ASSETS="${CMAKE_SOURCE_DIR}/assets/irs")
target_compile_features(bass_dsp_test PRIVATE cxx_std_20)
add_test(NAME bass_dsp COMMAND bass_dsp_test)

add_executable(bass_user_ir_test test/test_user_ir.cpp ${BASS_DSP_SOURCES})
target_include_directories(bass_user_ir_test PRIVATE
  "${CMAKE_SOURCE_DIR}/src"
  ${BASS_GUITARIX_INCLUDES})
target_link_libraries(bass_user_ir_test PRIVATE PkgConfig::SAMPLERATE nam_core Threads::Threads)
target_compile_features(bass_user_ir_test PRIVATE cxx_std_20)
add_test(NAME bass_user_ir COMMAND bass_user_ir_test)

add_executable(bass_nam_test test/test_nam.cpp)
target_include_directories(bass_nam_test PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(bass_nam_test PRIVATE PkgConfig::SAMPLERATE nam_core Threads::Threads)
target_compile_definitions(bass_nam_test PRIVATE
  NAM_TEST_MODEL="${CMAKE_SOURCE_DIR}/test/fixtures/wavenet.nam")
target_compile_features(bass_nam_test PRIVATE cxx_std_20)
add_test(NAME bass_nam COMMAND bass_nam_test)

add_executable(bass_panel_test test/test_panel.cpp ${BASS_UI_SOURCES})
target_include_directories(bass_panel_test PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(bass_panel_test PRIVATE PkgConfig::CAIRO PkgConfig::FREETYPE)
target_compile_features(bass_panel_test PRIVATE cxx_std_20)
add_test(NAME bass_panel COMMAND bass_panel_test)

add_executable(bass_layout_test test/test_layout.cpp ${BASS_UI_SOURCES})
target_include_directories(bass_layout_test PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(bass_layout_test PRIVATE PkgConfig::CAIRO PkgConfig::FREETYPE)
target_compile_features(bass_layout_test PRIVATE cxx_std_20)
add_test(NAME bass_layout COMMAND bass_layout_test)

add_executable(bass_geometry_test test/test_geometry.cpp)
target_include_directories(bass_geometry_test PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(bass_geometry_test PRIVATE PkgConfig::CAIRO)
target_compile_features(bass_geometry_test PRIVATE cxx_std_20)
add_test(NAME bass_geometry COMMAND bass_geometry_test)

add_executable(bass_ui_preview test/ui_preview.cpp ${BASS_UI_SOURCES})
target_include_directories(bass_ui_preview PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(bass_ui_preview PRIVATE PkgConfig::CAIRO PkgConfig::FREETYPE)
target_compile_features(bass_ui_preview PRIVATE cxx_std_20)

add_test(NAME bass_ttl
  COMMAND bass_ttl_gen --check --root "${CMAKE_SOURCE_DIR}")

pkg_check_modules(LILV IMPORTED_TARGET lilv-0)
if(LILV_FOUND)
  add_executable(bass_lv2_host_test test/lv2_host_test.cpp)
  target_include_directories(bass_lv2_host_test PRIVATE "${CMAKE_SOURCE_DIR}/src")
  target_link_libraries(bass_lv2_host_test PRIVATE PkgConfig::LILV PkgConfig::LV2)
  target_compile_features(bass_lv2_host_test PRIVATE cxx_std_20)
  add_dependencies(bass_lv2_host_test bass_lv2_bundle)
  add_test(NAME bass_lv2_host
    COMMAND bass_lv2_host_test "${CMAKE_BINARY_DIR}/lv2/bass-suite.lv2")
endif()

add_executable(bass_clap_host_test test/clap_host_test.cpp)
target_include_directories(bass_clap_host_test PRIVATE
  "${CMAKE_SOURCE_DIR}/src"
  "${CMAKE_SOURCE_DIR}/third_party/clap/include")
target_link_libraries(bass_clap_host_test PRIVATE ${CMAKE_DL_LIBS})
target_compile_features(bass_clap_host_test PRIVATE cxx_std_20)
add_dependencies(bass_clap_host_test bass_suite_bundle)
add_test(NAME bass_clap_host
  COMMAND bass_clap_host_test "${CMAKE_BINARY_DIR}/clap/BassSuite.clap")

add_executable(bass_clap_user_ir_test test/clap_user_ir_test.cpp)
target_include_directories(bass_clap_user_ir_test PRIVATE
  "${CMAKE_SOURCE_DIR}/src"
  "${CMAKE_SOURCE_DIR}/third_party/clap/include")
target_link_libraries(bass_clap_user_ir_test PRIVATE ${CMAKE_DL_LIBS})
target_compile_features(bass_clap_user_ir_test PRIVATE cxx_std_20)
add_test(NAME bass_clap_user_ir
  COMMAND bass_clap_user_ir_test "$<TARGET_FILE:bass_suite>")

add_executable(bass_clap_nam_test test/clap_nam_test.cpp)
target_include_directories(bass_clap_nam_test PRIVATE
  "${CMAKE_SOURCE_DIR}/src"
  "${CMAKE_SOURCE_DIR}/third_party/clap/include")
target_link_libraries(bass_clap_nam_test PRIVATE ${CMAKE_DL_LIBS})
target_compile_definitions(bass_clap_nam_test PRIVATE
  NAM_TEST_MODEL="${CMAKE_SOURCE_DIR}/test/fixtures/wavenet.nam")
target_compile_features(bass_clap_nam_test PRIVATE cxx_std_20)
add_test(NAME bass_clap_nam
  COMMAND bass_clap_nam_test "$<TARGET_FILE:bass_suite>")

add_executable(bass_tone_probe tools/bass_tone_probe.cpp)
target_include_directories(bass_tone_probe PRIVATE "${CMAKE_SOURCE_DIR}/src")
target_link_libraries(bass_tone_probe PRIVATE PkgConfig::SAMPLERATE nam_core Threads::Threads)
target_compile_features(bass_tone_probe PRIVATE cxx_std_20)

add_executable(bass_probe tools/bass_probe.cpp ${BASS_DSP_SOURCES})
target_include_directories(bass_probe PRIVATE
  "${CMAKE_SOURCE_DIR}/src"
  ${BASS_GUITARIX_INCLUDES})
target_link_libraries(bass_probe PRIVATE PkgConfig::SAMPLERATE nam_core Threads::Threads)
target_compile_features(bass_probe PRIVATE cxx_std_20)
