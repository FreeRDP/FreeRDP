set (BUILD_TESTING ON CACHE BOOL "qa default") 
set (CMAKE_VERBOSE_MAKEFILE ON CACHE BOOL "preload")
set (WITH_SAMPLE ON CACHE BOOL "qa default") 
set (WITH_SERVER ON CACHE BOOL "qa default")
set (WITH_SHADOW ON CACHE BOOL "qa default")
set (WITH_PROXY ON CACHE BOOL "qa default")
set (WITH_PULSE ON CACHE BOOL "qa default")
set (WITH_CUPS ON CACHE BOOL "qa default")
set (WITH_PCSC ON  CACHE BOOL "qa default")
set (WITH_SOXR ON  CACHE BOOL "qa default")
set (WITH_SWSCALE ON  CACHE BOOL "qa default")
set (WITH_DSP_FFMPEG ON CACHE BOOL "qa default")
set (WITH_FFMPEG ON CACHE BOOL "qa default")
set (WITH_SANITIZE_ADDRESS ON CACHE BOOL "qa default")
set (CMAKE_C_FLAGS "-Weverything -Wno-exit-time-destructors -Wno-cast-align -Wno-documentation -Wno-documentation-unknown-command -Wno-padded -Wno-covered-switch-default -Wno-declaration-after-statement" CACHE STRING "qa default")
set (CMAKE_CXX_FLAGS "-Weverything -Wno-exit-time-destructors -Wno-cast-align -Wno-documentation -Wno-documentation-unknown-command -Wno-padded -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-covered-switch-default -Wno-declaration-after-statement" CACHE STRING "qa default")

set (BUILD_WITH_CLANG_TIDY ON CACHE BOOL "qa default")
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
