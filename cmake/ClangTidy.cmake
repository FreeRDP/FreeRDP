option(BUILD_WITH_CLANG_TIDY "Build with clang-tidy for extra warnings" OFF)

if (BUILD_WITH_CLANG_TIDY)
    find_program(CLANG_TIDY_EXE
        NAMES
            clang-tidy-20
            clang-tidy-19
            clang-tidy-18
            clang-tidy-17
            clang-tidy-16
            clang-tidy-15
            clang-tidy-14
            clang-tidy-13
            clang-tidy-12
            clang-tidy-11
            clang-tidy-10
            clang-tidy
        REQUIRED
    )
    set(CLANG_TIDY_COMMAND "${CLANG_TIDY_EXE}" --config-file=${CMAKE_SOURCE_DIR}/.clang-tidy)

    set(CMAKE_C_CLANG_TIDY "${CLANG_TIDY_COMMAND}" --extra-arg=-std=gnu11)
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_COMMAND}" --extra-arg=-std=gnu++17)
    set(CMAKE_OBJC_CLANG_TIDY "${CLANG_TIDY_COMMAND}")
else()
    unset(CMAKE_C_CLANG_TIDY)
    unset(CMAKE_CXX_CLANG_TIDY)
    unset(CMAKE_OBJC_CLANG_TIDY)
endif()
