# This option allows deactivating FreeRDP supplied platform defaults to replace these with
# user supplied values.
#
# Compilation will fail without a replacement defining the symbols, but that can be
# done by supplying a TOOLCHAIN_FILE defining these.
option(USE_PLATFORM_DEFAULT
       "Use this configuration file for platform defaults. Supply -DCMAKE_TOOLCHAIN_FILE=<yourfile>." ON
)
if(USE_PLATFORM_DEFAULT)
  # default defines or other required preferences per platform
  if((CMAKE_SYSTEM_NAME MATCHES "WindowsStore") AND (CMAKE_SYSTEM_VERSION MATCHES "10.0"))
    set(UWP 1 CACHE BOOL "platform default")
    add_compile_definitions("_UWP")
    set(CMAKE_WINDOWS_VERSION "WIN10" CACHE STRING "platform default")
  endif()

  if("${CMAKE_SYSTEM_NAME}" MATCHES "Linux")
    # Linux already does define _POSIX_C_SOURCE by default, nothing to do
    add_compile_definitions("_FILE_OFFSET_BITS=64")
    set(WINPR_TIMEZONE_FILE "/etc/timezone")
  endif()

  if("${CMAKE_SYSTEM_NAME}" MATCHES "FreeBSD")
    set(BSD TRUE CACHE INTERNAL "platform default")
    set(FREEBSD TRUE CACHE INTERNAL "platform default")
    # we want POSIX 2008. FreeBSD 14 does only support 2001 fully, but the subset we require from 2008
    # is implemented, so ignore _POSIX_VERSION from unistd.h
    add_compile_definitions("_POSIX_C_SOURCE=200809L")
    # TODO: FreeBSD allows mixing POSIX and BSD API calls if we do not set
    # _POSIX_C_SOURCE but lack a macro to re-enable the BSD calls...
    add_compile_definitions("__BSD_VISIBLE")

    # There are some symbols only visible for XOpen standard
    add_compile_definitions("_XOPEN_SOURCE=700")
    add_compile_definitions("_FILE_OFFSET_BITS=64")
    set(WINPR_TIMEZONE_FILE "/var/db/zoneinfo")
  endif()

  if("${CMAKE_SYSTEM_NAME}" MATCHES "SunOS")
    # TODO: Does somebody still use this? please show yourself and
    # tell us if this still works.
    add_compile_definitions("_POSIX_PTHREAD_SEMANTICS")
    list(APPEND CMAKE_STANDARD_LIBRARIES rt)
    set(CMAKE_STANDARD_LIBRARIES ${CMAKE_STANDARD_LIBRARIES} CACHE STRING "platform default")
    set(WITH_SUN true CACHE BOOL "platform default")
  endif()

  if("${CMAKE_SYSTEM_NAME}" MATCHES "Darwin")
    # we want POSIX 2008. MacOS does only support 2001 fully, but the subset we require from 2008
    # is implemented, so ignore _POSIX_VERSION from unistd.h
    add_compile_definitions("_POSIX_C_SOURCE=200809L")

    # as _POSIX_C_SOURCE sets a fully POSIX confirmant environment re-enable
    # MacOS API visibility by defining the following feature test macro
    add_compile_definitions("_DARWIN_C_SOURCE")
  endif()

  if(${CMAKE_SYSTEM_NAME} MATCHES "kFreeBSD")
    set(BSD TRUE CACHE INTERNAL "platform default")
    set(KFREEBSD TRUE CACHE INTERNAL "platform default")
    add_compile_definitions(KFREEBSD)
    add_compile_definitions("_GNU_SOURCE")
  endif()

  if(${CMAKE_SYSTEM_NAME} MATCHES "OpenBSD")
    set(BSD TRUE CACHE INTERNAL "platform default")
    set(OPENBSD TRUE CACHE INTERNAL "platform default")
  endif()

  if(${CMAKE_SYSTEM_NAME} MATCHES "DragonFly")
    set(BSD TRUE CACHE INTERNAL "platform default")
    set(FREEBSD TRUE CACHE INTERNAL "platform default")

    # we want POSIX 2008. FreeBSD 14 does only support 2001 fully, but the subset we require from 2008
    # is implemented, so ignore _POSIX_VERSION from unistd.h
    add_compile_definitions("_POSIX_C_SOURCE=200809L")
    # TODO: FreeBSD allows mixing POSIX and BSD API calls if we do not set
    # _POSIX_C_SOURCE but lack a macro to re-enable the BSD calls...
    add_compile_definitions("__BSD_VISIBLE")

    # There are some symbols only visible for XOpen standard
    add_compile_definitions("_XOPEN_SOURCE=700")
    add_compile_definitions("_FILE_OFFSET_BITS=64")
    set(WINPR_TIMEZONE_FILE "/var/db/zoneinfo")
  endif()

  if(BSD)
    if(IS_DIRECTORY /usr/local/include)
      include_directories(SYSTEM /usr/local/include)
      link_directories(/usr/local/lib)
    endif()
    if(OPENBSD)
      if(IS_DIRECTORY /usr/X11R6/include)
        include_directories(SYSTEM /usr/X11R6/include)
      endif()
    endif()
  endif()

  # define a fallback for systems that do not support a timezone file or we did not yet test.
  # since most of these are BSD try a sensible default
  if(NOT WINPR_TIMEZONE_FILE)
    set(WINPR_TIMEZONE_FILE "/var/db/zoneinfo")
  endif()
  add_compile_definitions("WINPR_TIMEZONE_FILE=\"${WINPR_TIMEZONE_FILE}\"")

  if(FREEBSD)
    find_path(EPOLLSHIM_INCLUDE_DIR NAMES sys/epoll.h sys/timerfd.h HINTS /usr/local/include/libepoll-shim)
    find_library(EPOLLSHIM_LIBS NAMES epoll-shim libepoll-shim HINTS /usr/local/lib)
  endif()
endif()
