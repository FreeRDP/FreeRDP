set(CMAKE_SYSTEM_NAME Windows CACHE STRING "toolchain default")

set(CMAKE_SYSTEM_PROCESSOR amd64 CACHE STRING "toolchain default")

# https://github.com/meganz/mingw-std-threads
#
# we simply compile with the POSIX C++ primitives, but faster is using the wrapper
set(CMAKE_C_COMPILER /usr/bin/x86_64-w64-mingw32-gcc-posix CACHE STRING "toolchain default")
set(CMAKE_CXX_COMPILER /usr/bin/x86_64-w64-mingw32-g++-posix CACHE STRING "toolchain default")
set(CMAKE_RC_COMPILER_INIT /usr/bin/x86_64-w64-mingw32-windres CACHE STRING "toolchain default")
set(CMAKE_RC_COMPILER /usr/bin/x86_64-w64-mingw32-windres CACHE STRING "toolchain default")
set(CMAKE_AR /usr/bin/x86_64-w64-mingw32-gcc-ar-posix CACHE STRING "toolchain default")
set(CMAKE_C_COMPILER_AR /usr/bin/x86_64-w64-mingw32-gcc-ar-posix CACHE STRING "toolchain default")
set(CMAKE_CXX_COMPILER_AR /usr/bin/x86_64-w64-mingw32-gcc-ar-posix CACHE STRING "toolchain default")
set(CMAKE_RANLIB /usr/bin/x86_64-w64-mingw32-gcc-ranlib-posix CACHE STRING "toolchain default")
set(CMAKE_C_COMPILER_RANLIB /usr/bin/x86_64-w64-mingw32-gcc-ranlib-posix CACHE STRING "toolchain default")
set(CMAKE_CXX_COMPILER_RANLIB /usr/bin/x86_64-w64-mingw32-gcc-ranlib-posix CACHE STRING "toolchain default")
set(CMAKE_LINKER /usr/bin/x86_64-w64-mingw32-ld-posix CACHE STRING "toolchain default")
set(CMAKE_NM /usr/bin/x86_64-w64-mingw32-nm-posix CACHE STRING "toolchain default")
set(CMAKE_READELF /usr/bin/x86_64-w64-mingw32-readelf CACHE STRING "toolchain default")
set(CMAKE_OBJCOPY /usr/bin/x86_64-w64-mingw32-objcopy CACHE STRING "toolchain default")
set(CMAKE_OBJDUMP /usr/bin/x86_64-w64-mingw32-objdump CACHE STRING "toolchain default")

set(CMAKE_SYSROOT /usr/x86_64-w64-mingw32 CACHE STRING "toolchain default")

#set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER CACHE STRING "toolchain default")
#set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY CACHE STRING "toolchain default")
#set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY CACHE STRING "toolchain default")
#set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY CACHE STRING "toolchain default")

set(CMAKE_WINDOWS_VERSION "WIN10" CACHE STRING "toolchain default")
set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING "toolchain default")
set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY ON CACHE BOOL "toolchain default")
set(CMAKE_VERBOSE_MAKEFILE ON CACHE BOOL "toolchain default")
set(THREADS_PREFER_PTHREAD_FLAG ON CACHE BOOL "toolchain default")
