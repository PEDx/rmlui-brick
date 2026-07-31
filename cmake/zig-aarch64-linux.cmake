set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_SKIP_RPATH TRUE)

get_filename_component(PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(TRIMUI_SYSROOT "${PROJECT_ROOT}/.deps/tg5040-sysroot" CACHE PATH "TrimUI SDK sysroot")

if(NOT EXISTS "${TRIMUI_SYSROOT}/usr/include/SDL2/SDL.h")
  message(FATAL_ERROR "TrimUI SDK is missing. Run scripts/setup-toolchain.sh first.")
endif()

set(CMAKE_C_COMPILER "${PROJECT_ROOT}/scripts/zig-cc-aarch64-linux.sh")
set(CMAKE_CXX_COMPILER "${PROJECT_ROOT}/scripts/zig-cxx-aarch64-linux.sh")
set(CMAKE_AR "${PROJECT_ROOT}/scripts/zig-ar.sh")
set(CMAKE_RANLIB "${PROJECT_ROOT}/scripts/zig-ranlib.sh")

set(CMAKE_FIND_ROOT_PATH "${TRIMUI_SYSROOT}/usr")
set(CMAKE_PREFIX_PATH "${TRIMUI_SYSROOT}/usr")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set(CMAKE_C_FLAGS_INIT "-isystem ${TRIMUI_SYSROOT}/usr/include -fno-sanitize=all")
set(CMAKE_CXX_FLAGS_INIT "-isystem ${TRIMUI_SYSROOT}/usr/include -fno-sanitize=all")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-L${TRIMUI_SYSROOT}/usr/lib -Wl,-rpath-link,${TRIMUI_SYSROOT}/usr/lib")

set(SDL2_INCLUDE_DIR "${TRIMUI_SYSROOT}/usr/include/SDL2" CACHE PATH "SDL2 includes")
set(SDL2_LIBRARY "${TRIMUI_SYSROOT}/usr/lib/libSDL2.so" CACHE FILEPATH "SDL2 library")
set(FREETYPE_INCLUDE_DIRS
  "${TRIMUI_SYSROOT}/usr/include/freetype2;${TRIMUI_SYSROOT}/usr/include"
  CACHE STRING "FreeType includes")
set(FREETYPE_LIBRARY "${TRIMUI_SYSROOT}/usr/lib/libfreetype.so" CACHE FILEPATH "FreeType library")
