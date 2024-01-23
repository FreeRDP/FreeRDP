message("PRELOADING cache")
set (CMAKE_VERBOSE_MAKEFILE ON CACHE BOOL "preload")
set (WITH_SERVER ON CACHE BOOL "qa default")
set (WITH_SAMPLE ON CACHE BOOL "qa default")
set (WITH_VERBOSE_WINPR_ASSERT OFF CACHE BOOL "qa default")
set (BUILD_SHARED_LIBS OFF CACHE BOOL "qa default")

set (BUILD_WITH_CLANG_TIDY OFF CACHE BOOL "qa default")
find_program(CLANG_EXE
    NAMES
        clang-20
        clang-19
        clang-18
        clang-17
        clang-16
        clang-15
        clang-14
        clang-13
        clang-12
        clang-11
        clang-10
        clang
    REQUIRED
)
set (CMAKE_C_COMPILER "${CLANG_EXE}" CACHE STRING "qa default")

find_program(CLANG_XX_EXE
    NAMES
        clang++-20
        clang++-19
        clang++-18
        clang++-17
        clang++-16
        clang++-15
        clang++-14
        clang++-13
        clang++-12
        clang++-11
        clang++-10
        clang++
    REQUIRED
)
set (CMAKE_CXX_COMPILER "${CLANG_XX_EXE}" CACHE STRING "qa default")
