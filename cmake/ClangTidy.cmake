option(BUILD_WITH_CLANG_TIDY "Build with clang-tidy for extra warnings" OFF)

if(BUILD_WITH_CLANG_TIDY)
  include(ClangDetectTool)
  clang_detect_tool(CLANG_TIDY_EXE clang-tidy REQUIRED)

  set(CLANG_TIDY_COMMAND "${CLANG_TIDY_EXE}")

  set(CMAKE_C_CLANG_TIDY "${CLANG_TIDY_COMMAND}" --extra-arg=-std=gnu11)
  set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_COMMAND}" --extra-arg=-std=gnu++17)
  set(CMAKE_OBJC_CLANG_TIDY "${CLANG_TIDY_COMMAND}")
else()
  unset(CMAKE_C_CLANG_TIDY)
  unset(CMAKE_CXX_CLANG_TIDY)
  unset(CMAKE_OBJC_CLANG_TIDY)
endif()
