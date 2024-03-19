# default defines or other required preferences per platform
if((CMAKE_SYSTEM_NAME MATCHES "WindowsStore") AND (CMAKE_SYSTEM_VERSION MATCHES "10.0"))
    set(UWP 1 CACHE BOOL "platform default")
	add_definitions("-D_UWP")
    set(CMAKE_WINDOWS_VERSION "WIN10" CACHE STRING "platform default")
endif()
# Enable 64bit file support on linux and FreeBSD.
if("${CMAKE_SYSTEM_NAME}" MATCHES "Linux")
	add_definitions("-D_FILE_OFFSET_BITS=64")
endif()

if("${CMAKE_SYSTEM_NAME}" MATCHES FREEBSD)
	add_definitions("-D_FILE_OFFSET_BITS=64")
endif()

# Use Standard conforming getpwnam_r() on Solaris.
# Use Standard conforming getpwnam_r() on Solaris.
if("${CMAKE_SYSTEM_NAME}" MATCHES "SunOS")
	add_definitions("-D_POSIX_PTHREAD_SEMANTICS")
    list(APPEND CMAKE_STANDARD_LIBRARIES rt)
    set(CMAKE_STANDARD_LIBRARIES ${CMAKE_STANDARD_LIBRARIES} CACHE STRING "platform default")
    set(WITH_SUN true CACHE BOOL "platform default")
endif()
