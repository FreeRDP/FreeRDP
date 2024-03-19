# default defines or other required preferences per platform
if((CMAKE_SYSTEM_NAME MATCHES "WindowsStore") AND (CMAKE_SYSTEM_VERSION MATCHES "10.0"))
    set(UWP 1 CACHE BOOL "platform default")
	add_definitions("-D_UWP")
    set(CMAKE_WINDOWS_VERSION "WIN10" CACHE STRING "platform default")
endif()

if("${CMAKE_SYSTEM_NAME}" MATCHES "Linux")
	add_definitions("-D_FILE_OFFSET_BITS=64")
    add_definitions("-DWINPR_TIMEZONE_FILE=\"/etc/timezone\"")
endif()

if("${CMAKE_SYSTEM_NAME}" MATCHES "FreeBSD")
	add_definitions("-D_FILE_OFFSET_BITS=64")
    add_definitions("-DWINPR_TIMEZONE_FILE=\"/var/db/zoneinfo\"")
endif()

if("${CMAKE_SYSTEM_NAME}" MATCHES "SunOS")
	add_definitions("-D_POSIX_PTHREAD_SEMANTICS")
    list(APPEND CMAKE_STANDARD_LIBRARIES rt)
    set(CMAKE_STANDARD_LIBRARIES ${CMAKE_STANDARD_LIBRARIES} CACHE STRING "platform default")
    set(WITH_SUN true CACHE BOOL "platform default")
endif()
