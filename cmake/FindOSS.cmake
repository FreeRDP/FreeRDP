#
# Find OSS include header for Unix platforms.
# used by FQTerm to detect the availability of OSS.

if(UNIX)
  if(CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(PLATFORM_PREFIX "linux/")
  elseif(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
    set(PLATFORM_PREFIX "sys/")
  elseif(CMAKE_SYSTEM_NAME MATCHES "OpenBSD")
    set(PLATFORM_PREFIX "machine/")
  endif()
endif(UNIX)

set(OSS_HDR_NAME "${PLATFORM_PREFIX}soundcard.h" CACHE STRING "oss header include file name")
find_path(OSS_INCLUDE_DIRS ${OSS_HDR_NAME} PATHS "/usr/local/include" PATH_SUFFIXES ${PLATFORM_SUFFIX})

if(OSS_INCLUDE_DIRS)
  set(OSS_FOUND ON CACHE BOOL "oss detection status")
else(OSS_INCLUDE_DIRS)
  set(OSS_FOUND OFF CACHE BOOL "oss detection status")
endif(OSS_INCLUDE_DIRS)

if(OSS_FOUND)
  message(STATUS "Found OSS Audio")
else(OSS_FOUND)
  if(OSS_FIND_REQUIRED)
    message(FATAL_ERROR "FAILED to found Audio - REQUIRED")
  else(OSS_FIND_REQUIRED)
    message(STATUS "Audio Disabled")
  endif(OSS_FIND_REQUIRED)
endif(OSS_FOUND)

mark_as_advanced(OSS_FOUND OSS_HDR_NAME OSS_INCLUDE_DIRS)
