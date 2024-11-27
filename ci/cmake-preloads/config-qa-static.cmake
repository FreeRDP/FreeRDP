message("PRELOADING cache")
set(CMAKE_VERBOSE_MAKEFILE ON CACHE BOOL "preload")
set(WITH_SERVER ON CACHE BOOL "qa default")
set(WITH_SAMPLE ON CACHE BOOL "qa default")
set(WITH_SIMD ON CACHE BOOL "qa default")
set(WITH_VERBOSE_WINPR_ASSERT OFF CACHE BOOL "qa default")
set(ENABLE_WARNING_VERBOSE ON CACHE BOOL "preload")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "qa default")

set(BUILD_WITH_CLANG_TIDY OFF CACHE BOOL "qa default")

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/ClangDetectTool.cmake)
clang_detect_tool(CLANG_EXE clang REQUIRED)
clang_detect_tool(CLANG_XX_EXE clang++ REQUIRED)

set(CMAKE_C_COMPILER "${CLANG_EXE}" CACHE STRING "qa default")
set(CMAKE_CXX_COMPILER "${CLANG_XX_EXE}" CACHE STRING "qa default")
set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++" CACHE STRING "qa-default")
