if($ENV{CLANG_VERSION})
  set(CLANG_VERSION "-$ENV{CLANG_VERSION}")
endif()

set(CLANG_WARNINGS
    "-pedantic -Weverything -Wno-padded -Wno-covered-switch-default -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-cast-align"
)

set(CMAKE_C_COMPILER "/usr/bin/clang${CLANG_VERSION}" CACHE PATH "")
set(CMAKE_C_FLAGS ${CLANG_WARNINGS} CACHE STRING "")

set(CMAKE_CXX_COMPILER "/usr/bin/clang++${CLANG_VERSION}" CACHE PATH "")
set(CMAKE_CXX_FLAGS ${CLANG_WARNINGS} CACHE STRING "")

set(CMAKE_AR "/usr/bin/llvm-ar${CLANG_VERSION}" CACHE PATH "")
set(CMAKE_LINKER "/usr/bin/llvm-link${CLANG_VERSION}" CACHE PATH "")
set(CMAKE_NM "/usr/bin/llvm-nm${CLANG_VERSION}" CACHE PATH "")
set(CMAKE_OBJDUMP "/usr/bin/llvm-objdump${CLANG_VERSION}" CACHE PATH "")
set(CMAKE_RANLIB "/usr/bin/llvm-ranlib${CLANG_VERSION}" CACHE PATH "")
