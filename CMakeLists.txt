cmake_minimum_required(VERSION 3.20)

project(trimui_rmlui_prototype VERSION 0.1.0 LANGUAGES C CXX)

include(FetchContent)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static dependencies" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "Disable dependency tests" FORCE)
set(RMLUI_SAMPLES OFF CACHE BOOL "Disable RmlUi samples" FORCE)
set(RMLUI_LUA_BINDINGS OFF CACHE BOOL "Disable Lua" FORCE)
set(RMLUI_LOTTIE_PLUGIN OFF CACHE BOOL "Disable Lottie" FORCE)
set(RMLUI_SVG_PLUGIN OFF CACHE BOOL "Disable SVG" FORCE)
set(RMLUI_HARFBUZZ_SAMPLE OFF CACHE BOOL "Disable HarfBuzz sample" FORCE)
set(RMLUI_PRECOMPILED_HEADERS OFF CACHE BOOL "Keep cross builds simple" FORCE)
set(RMLUI_COMPILER_OPTIONS OFF CACHE BOOL "Use prototype compiler flags" FORCE)

FetchContent_Declare(
  rmlui
  GIT_REPOSITORY https://github.com/mikke89/RmlUi.git
  GIT_TAG 2230d1a6e8e0848ed87a5761e2a5160b2a175ba4
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(rmlui)

find_package(SDL2 QUIET CONFIG)
if(NOT TARGET SDL2::SDL2)
  find_path(SDL2_INCLUDE_DIR SDL.h PATH_SUFFIXES SDL2 REQUIRED)
  find_library(SDL2_LIBRARY NAMES SDL2 REQUIRED)
  add_library(SDL2::SDL2 UNKNOWN IMPORTED)
  set_target_properties(SDL2::SDL2 PROPERTIES
    IMPORTED_LOCATION "${SDL2_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${SDL2_INCLUDE_DIR}"
  )
endif()

find_package(SDL2_image QUIET CONFIG)
if(NOT TARGET SDL2_image::SDL2_image)
  find_path(SDL2_IMAGE_INCLUDE_DIR SDL_image.h PATH_SUFFIXES SDL2 REQUIRED)
  find_library(SDL2_IMAGE_LIBRARY NAMES SDL2_image REQUIRED)
  add_library(SDL2_image::SDL2_image UNKNOWN IMPORTED)
  set_target_properties(SDL2_image::SDL2_image PROPERTIES
    IMPORTED_LOCATION "${SDL2_IMAGE_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${SDL2_IMAGE_INCLUDE_DIR}"
  )
endif()

add_executable(trimui-rmlui-prototype
  src/game_catalog.cpp
  src/main.cpp
  src/prototype_render_interface.cpp
  src/prototype_system_interface.cpp
)

target_compile_features(trimui-rmlui-prototype PRIVATE cxx_std_14)
target_compile_definitions(trimui-rmlui-prototype PRIVATE SDL_MAIN_HANDLED)
target_link_libraries(
  trimui-rmlui-prototype
  PRIVATE RmlUi::Core RmlUi::Debugger SDL2::SDL2 SDL2_image::SDL2_image
)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(trimui-rmlui-prototype PRIVATE -Wall -Wextra -Wpedantic)
endif()

install(TARGETS trimui-rmlui-prototype RUNTIME DESTINATION bin)
install(DIRECTORY assets/ DESTINATION assets)
