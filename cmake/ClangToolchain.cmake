if ($ENV{CLANG_VERSION})
	SET (CLANG_VERSION "-$ENV{CLANG_VERSION}")
endif()

SET (CLANG_WARNINGS "-pedantic -Weverything -Wno-padded -Wno-covered-switch-default -Wno-c++98-compat -Wno-c++98-compat-pedantic -Wno-cast-align")

SET (CMAKE_C_COMPILER             "/usr/bin/clang${CLANG_VERSION}" CACHE PATH "")
SET (CMAKE_C_FLAGS                ${CLANG_WARNINGS} CACHE STRING "")

SET (CMAKE_CXX_COMPILER             "/usr/bin/clang++${CLANG_VERSION}" CACHE PATH "")
SET (CMAKE_CXX_FLAGS                ${CLANG_WARNINGS} CACHE STRING "")

SET (CMAKE_AR      "/usr/bin/llvm-ar${CLANG_VERSION}" CACHE PATH "")
SET (CMAKE_LINKER  "/usr/bin/llvm-link${CLANG_VERSION}" CACHE PATH "")
SET (CMAKE_NM      "/usr/bin/llvm-nm${CLANG_VERSION}" CACHE PATH "")
SET (CMAKE_OBJDUMP "/usr/bin/llvm-objdump${CLANG_VERSION}" CACHE PATH "")
SET (CMAKE_RANLIB  "/usr/bin/llvm-ranlib${CLANG_VERSION}" CACHE PATH "")
