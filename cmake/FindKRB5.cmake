# - Try to find the Kerberos libraries
# Once done this will define
#
#  KRB_ROOT_DIR - Set this variable to the root installation of Kerberos
#  KRB_ROOT_FLAVOUR - Set this variable to the flavour of Kerberos installation (MIT or Heimdal)
#
# Read-Only variables:
#  KRB5_FOUND - system has the Heimdal library
#  KRB5_FLAVOUR - "MIT" or "Heimdal" if anything found.
#  KRB5_INCLUDE_DIR - the Heimdal include directory
#  KRB5_LIBRARIES - The libraries needed to use Kerberos
#  KRB5_LINK_DIRECTORIES - Directories to add to linker search path
#  KRB5_LINKER_FLAGS - Additional linker flags
#  KRB5_COMPILER_FLAGS - Additional compiler flags
#  KRB5_VERSION - This is set to version advertised by pkg-config or read from manifest.
#                In case the library is found but no version info availabe it'll be set to "unknown"

set(_MIT_MODNAME mit-krb5)
set(_HEIMDAL_MODNAME heimdal-krb5)

include(CheckIncludeFile)
include(CheckIncludeFiles)
include(CheckTypeSize)

# export KRB_ROOT_FLAVOUR to use pkg-config system under UNIX
if (NOT KRB_ROOT_FLAVOUR)
  set(KRB_ROOT_FLAVOUR $ENV{KRB_ROOT_FLAVOUR})
endif()

if (NOT KRB_ROOT_DIR)
  set(KRB_ROOT_DIR $ENV{KRB_ROOT_DIR})
endif()

if(UNIX)
  if(NOT "${KRB_ROOT_FLAVOUR} " STREQUAL " ")
    string(REGEX MATCH "^[M|m]it$" MIT_FLAVOUR "${KRB_ROOT_FLAVOUR}")
    if(NOT MIT_FLAVOUR)
      string(REGEX MATCH "^MIT$" MIT_FLAVOUR "${KRB_ROOT_FLAVOUR}")
    endif()
    string(REGEX MATCH "^[H|h]eimdal$" HEIMDAL_FLAVOUR "${KRB_ROOT_FLAVOUR}")
    if(NOT HEIMDAL_FLAVOUR)
      string(REGEX MATCH "^HEIMDAL$" HEIMDAL_FLAVOUR "${KRB_ROOT_FLAVOUR}")
    endif()
    if(MIT_FLAVOUR)
      set(KRB5_FLAVOUR "MIT")
    elseif(HEIMDAL_FLAVOUR)
      set(KRB5_FLAVOUR "Heimdal")
    else()
      message(SEND_ERROR "Kerberos flavour unknown (${KRB_ROOT_FLAVOUR}). Choose MIT or Heimdal.")
    endif()
  endif()
endif()

set(_KRB5_ROOT_HINTS
    "${KRB_ROOT_DIR}"
)

# try to find library using system pkg-config if user did not specify root dir
if(UNIX)
  if("${KRB_ROOT_DIR} " STREQUAL " ")
    if (NOT KRB5_FLAVOUR)
      message("KRB5_FLAVOUR not defined, defaulting to MIT")
      set(KRB5_FLAVOUR "MIT")
    endif()

    if(KRB5_FLAVOUR)
      find_package(PkgConfig QUIET)
      if(KRB5_FLAVOUR STREQUAL "MIT")
        pkg_search_module(_KRB5_PKG ${_MIT_MODNAME})
      else()
        pkg_search_module(_KRB5_PKG ${_HEIMDAL_MODNAME})
      endif()

      if("${_KRB5_PKG_PREFIX} " STREQUAL " ")
        if(NOT "$ENV{PKG_CONFIG_PATH} " STREQUAL " ")
          list(APPEND _KRB5_ROOT_HINTS "$ENV{PKG_CONFIG_PATH}")
        else()
          message(WARNING "pkg_search_module failed : try to set PKG_CONFIG_PATH to PREFIX_OF_KERBEROS/lib/pkgconfig")
        endif()
      else()
        if("${KRB_ROOT_FLAVOUR}" STREQUAL "Heimdal")
          string(FIND "${_KRB5_PKG_PREFIX}" "heimdal" PKG_HEIMDAL_PREFIX_POSITION)
          if(PKG_HEIMDAL_PREFIX_POSITION STREQUAL "-1")
            message(WARNING "Try to set PKG_CONFIG_PATH to \"PREFIX_OF_KERBEROS/lib/pkgconfig\"")
          else()
            list(APPEND _KRB5_ROOT_HINTS "${_KRB5_PKG_PREFIX}")
          endif()
        else()
          list(APPEND _KRB5_ROOT_HINTS "${_KRB5_PKG_PREFIX}")
        endif()
      endif()
    else()
      message(WARNING "export KRB_ROOT_FLAVOUR to use pkg-config")
    endif()
  endif()
elseif(WIN32)
  list(APPEND _KRB5_ROOT_HINTS "[HKEY_LOCAL_MACHINE\\SOFTWARE\\MIT\\Kerberos;InstallDir]")
endif()

if(NOT KRB5_FOUND) # not found by pkg-config. Let's take more traditional approach.
  if(KRB5_FLAVOUR STREQUAL "MIT")
    set(_KRB5_CONFIGURE_SCRIPT_SUFFIX ".mit")
  elseif(KRB5_FLAVOUR STREQUAL "Heimdal")
    set(_KRB5_CONFIGURE_SCRIPT_SUFFIX ".heimdal")
  endif()

  find_file(_KRB5_CONFIGURE_SCRIPT
      NAMES
        "krb5-config${_KRB5_CONFIGURE_SCRIPT_SUFFIX}"
        "krb5-config"
      HINTS
          ${_KRB5_ROOT_HINTS}
      PATH_SUFFIXES
          bin
      NO_CMAKE_PATH
      NO_CMAKE_ENVIRONMENT_PATH
  )

  if (${_KRB5_CONFIGURE_SCRIPT} STREQUAL "_KRB5_CONFIGURE_SCRIPT-NOTFOUND")
    # if not found in user-supplied directories, maybe system knows better
    find_file(_KRB5_CONFIGURE_SCRIPT
        NAMES
      "krb5-config${_KRB5_CONFIGURE_SCRIPT_SUFFIX}"
        PATH_SUFFIXES
      bin
    )
  endif()

  if (NOT ${_KRB5_CONFIGURE_SCRIPT} STREQUAL "_KRB5_CONFIGURE_SCRIPT-NOTFOUND")
    execute_process(
         COMMAND ${_KRB5_CONFIGURE_SCRIPT} "--vendor"
         OUTPUT_VARIABLE _KRB5_VENDOR
         RESULT_VARIABLE _KRB5_CONFIGURE_FAILED
    )
  else()
    set(_KRB5_CONFIGURE_FAILED 1)
  endif()

  if(NOT _KRB5_CONFIGURE_FAILED)
    string(STRIP "${_KRB5_VENDOR}" _KRB5_VENDOR)
    if(   (KRB5_FLAVOUR STREQUAL "Heimdal" AND NOT _KRB5_VENDOR STREQUAL "Heimdal")
       OR (KRB5_FLAVOUR STREQUAL "MIT" AND NOT _KRB5_VENDOR STREQUAL "Apple MITKerberosShim")
       OR (KRB5_FLAVOUR STREQUAL "MIT" AND NOT _KRB5_VENDOR STREQUAL "Massachusetts Institute of Technology"))
      message(WARNING "Kerberos vendor and Kerberos flavour are not matching : _KRB5_VENDOR=${_KRB5_VENDOR} ; KRB5_FLAVOUR=${KRB5_FLAVOUR}")
      message(STATUS "Try to set the path to Kerberos root folder in the system variable KRB_ROOT_DIR")
    endif()
  else()
    message(SEND_ERROR "Kerberos configure script failed to get vendor")
  endif()

  # NOTE: fail to link Heimdal libraries using configure script due to limitations
  # during Heimdal linking process. Then, we do it "manually".
  if(NOT "${_KRB5_CONFIGURE_SCRIPT} " STREQUAL " " AND KRB5_FLAVOUR AND NOT _KRB5_VENDOR STREQUAL "Heimdal")
    execute_process(
          COMMAND ${_KRB5_CONFIGURE_SCRIPT} "--cflags"
          OUTPUT_VARIABLE _KRB5_CFLAGS
          RESULT_VARIABLE _KRB5_CONFIGURE_FAILED
    )

    if(NOT _KRB5_CONFIGURE_FAILED) # 0 means success
      # should also work in an odd case when multiple directories are given
      string(STRIP "${_KRB5_CFLAGS}" _KRB5_CFLAGS)
      string(REGEX REPLACE " +-I" ";" _KRB5_CFLAGS "${_KRB5_CFLAGS}")
      string(REGEX REPLACE " +-([^I][^ \\t;]*)" ";-\\1" _KRB5_CFLAGS "${_KRB5_CFLAGS}")

      foreach(_flag ${_KRB5_CFLAGS})
        if(_flag MATCHES "^-I.*")
          string(REGEX REPLACE "^-I" "" _val "${_flag}")
          list(APPEND _KRB5_INCLUDE_DIR "${_val}")
        else()
          list(APPEND _KRB5_COMPILER_FLAGS "${_flag}")
        endif()
      endforeach()
    endif()

    if(_KRB5_VENDOR STREQUAL "Apple MITKerberosShim")
      execute_process(
            COMMAND ${_KRB5_CONFIGURE_SCRIPT} "--libs"
            OUTPUT_VARIABLE _KRB5_LIB_FLAGS
            RESULT_VARIABLE _KRB5_CONFIGURE_FAILED
      )
    elseif(_KRB5_VENDOR STREQUAL "Massachusetts Institute of Technology")
      execute_process(
            COMMAND ${_KRB5_CONFIGURE_SCRIPT} "--libs"
            OUTPUT_VARIABLE _KRB5_LIB_FLAGS
            RESULT_VARIABLE _KRB5_CONFIGURE_FAILED
      )
    elseif(_KRB5_VENDOR STREQUAL "Heimdal")
      execute_process(
            COMMAND ${_KRB5_CONFIGURE_SCRIPT} "--deps --libs" 
            OUTPUT_VARIABLE _KRB5_LIB_FLAGS
            RESULT_VARIABLE _KRB5_CONFIGURE_FAILED
      )
    else()
      message(SEND_ERROR "Unknown vendor '${_KRB5_VENDOR}'")
    endif()

    if(NOT _KRB5_CONFIGURE_FAILED) # 0 means success
      # this script gives us libraries and link directories. We have to deal with it.
      string(STRIP "${_KRB5_LIB_FLAGS}" _KRB5_LIB_FLAGS)
      string(REGEX REPLACE " +-(L|l)" ";-\\1" _KRB5_LIB_FLAGS "${_KRB5_LIB_FLAGS}")
      string(REGEX REPLACE " +-([^Ll][^ \\t;]*)" ";-\\1" _KRB5_LIB_FLAGS "${_KRB5_LIB_FLAGS}")

      foreach(_flag ${_KRB5_LIB_FLAGS})
        if(_flag MATCHES "^-l.*")
          string(REGEX REPLACE "^-l" "" _val "${_flag}")
          list(APPEND _KRB5_LIBRARIES "${_val}")
        elseif(_flag MATCHES "^-L.*")
          string(REGEX REPLACE "^-L" "" _val "${_flag}")
          list(APPEND _KRB5_LINK_DIRECTORIES "${_val}")
        else()
          list(APPEND _KRB5_LINKER_FLAGS "${_flag}")
        endif()
      endforeach()

    endif()

    execute_process(
          COMMAND ${_KRB5_CONFIGURE_SCRIPT} "--version"
          OUTPUT_VARIABLE _KRB5_VERSION
          RESULT_VARIABLE _KRB5_CONFIGURE_FAILED
    )

    # older versions may not have the "--version" parameter. In this case we just don't care.
    if(_KRB5_CONFIGURE_FAILED)
      set(_KRB5_VERSION 0)
    endif()

  else() # either there is no config script or we are on platform that doesn't provide one (Windows?)
    if(_KRB5_VENDOR STREQUAL "Massachusetts Institute of Technology")
      find_path(_KRB5_INCLUDE_DIR
               NAMES
                   "gssapi/gssapi_generic.h"
               HINTS
                   ${_KRB5_ROOT_HINTS}
               PATH_SUFFIXES
                   include
                   inc
      )

      if(_KRB5_INCLUDE_DIR) # we've found something
        set(CMAKE_REQUIRED_INCLUDES "${_KRB5_INCLUDE_DIR}")
        check_include_files( "krb5/krb5.h" _KRB5_HAVE_MIT_HEADERS)
        if(_KRB5_HAVE_MIT_HEADERS)
          set(KRB5_FLAVOUR "MIT")
        else()
          message(SEND_ERROR "Try to set the Kerberos flavour (KRB_ROOT_FLAVOUR)")
        endif()
      elseif("$ENV{PKG_CONFIG_PATH} " STREQUAL " ")
        message(WARNING "Try to set PKG_CONFIG_PATH to PREFIX_OF_KERBEROS/lib/pkgconfig")
      endif()
    elseif(_KRB5_VENDOR STREQUAL "Heimdal")
      find_path(_KRB5_INCLUDE_DIR
               NAMES
                   "gssapi/gssapi_spnego.h"
               HINTS
                   ${_KRB5_ROOT_HINTS}
               PATHS
                   /usr/heimdal
                   /usr/local/heimdal
               PATH_SUFFIXES
                   include
                   inc
      )

      if(_KRB5_INCLUDE_DIR) # we've found something
        set(CMAKE_REQUIRED_INCLUDES "${_KRB5_INCLUDE_DIR}")
        # prevent compiling the header - just check if we can include it
        set(CMAKE_REQUIRED_DEFINITIONS "${CMAKE_REQUIRED_DEFINITIONS} -D__ROKEN_H__")
        check_include_file( "roken.h" _KRB5_HAVE_ROKEN_H)
        check_include_file( "heimdal/roken.h" _KRB5_HAVE_HEIMDAL_ROKEN_H)
        if(_KRB5_HAVE_ROKEN_H OR _KRB5_HAVE_HEIMDAL_ROKEN_H)
          set(KRB5_FLAVOUR "Heimdal")
        endif()
        set(CMAKE_REQUIRED_DEFINITIONS "")
      elseif("$ENV{PKG_CONFIG_PATH} " STREQUAL " ")
        message(WARNING "Try to set PKG_CONFIG_PATH to PREFIX_OF_KERBEROS/lib/pkgconfig")
      endif()
    else()
      message(SEND_ERROR "Kerberos vendor unknown (${_KRB5_VENDOR})")
    endif()

    # if we have headers, check if we can link libraries
    if(KRB5_FLAVOUR)
      set(_KRB5_LIBDIR_SUFFIXES "")
      set(_KRB5_LIBDIR_HINTS ${_KRB5_ROOT_HINTS})
      get_filename_component(_KRB5_CALCULATED_POTENTIAL_ROOT "${_KRB5_INCLUDE_DIR}" PATH)
      list(APPEND _KRB5_LIBDIR_HINTS ${_KRB5_CALCULATED_POTENTIAL_ROOT})

      if(WIN32)
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
          list(APPEND _KRB5_LIBDIR_SUFFIXES "lib/AMD64")
          if(KRB5_FLAVOUR STREQUAL "MIT")
            set(_KRB5_LIBNAME "krb5_64")
          else()
            set(_KRB5_LIBNAME "libkrb5")
          endif()
        else()
          list(APPEND _KRB5_LIBDIR_SUFFIXES "lib/i386")
          if(KRB5_FLAVOUR STREQUAL "MIT")
            set(_KRB5_LIBNAME "krb5_32")
          else()
            set(_KRB5_LIBNAME "libkrb5")
          endif()
        endif()
      else()
        list(APPEND _KRB5_LIBDIR_SUFFIXES "lib;lib64;x86_64-linux-gnu") # those suffixes are not checked for HINTS
        if(KRB5_FLAVOUR STREQUAL "MIT")
          set(_KRB5_LIBNAME "krb5")
          set(_COMERR_LIBNAME "com_err")
          set(_KRB5SUPPORT_LIBNAME "krb5support")
        else()
          set(_KRB5_LIBNAME "krb5")
          set(_ROKEN_LIBNAME "roken")
        endif()
      endif()

      if(KRB5_FLAVOUR STREQUAL "MIT")
        find_library(_KRB5_LIBRARY
                    NAMES
                        ${_KRB5_LIBNAME}
                    HINTS
                        ${_KRB5_LIBDIR_HINTS}
                    PATH_SUFFIXES
                        ${_KRB5_LIBDIR_SUFFIXES}
        )
        find_library(_COMERR_LIBRARY
                    NAMES
                        ${_COMERR_LIBNAME}
                    HINTS
                        ${_KRB5_LIBDIR_HINTS}
                    PATH_SUFFIXES
                        ${_KRB5_LIBDIR_SUFFIXES}
        )
        find_library(_KRB5SUPPORT_LIBRARY
                    NAMES
                        ${_KRB5SUPPORT_LIBNAME}
                    HINTS
                        ${_KRB5_LIBDIR_HINTS}
                    PATH_SUFFIXES
                        ${_KRB5_LIBDIR_SUFFIXES}
        )
        list(APPEND _KRB5_LIBRARIES ${_KRB5_LIBRARY} ${_KRB5SUPPORT_LIBRARY} ${_COMERR_LIBRARY})
      endif()

      if(KRB5_FLAVOUR STREQUAL "Heimdal")
        find_library(_KRB5_LIBRARY
                    NAMES
                        ${_KRB5_LIBNAME}
                    HINTS
                        ${_KRB5_LIBDIR_HINTS}
                    PATH_SUFFIXES
                        ${_KRB5_LIBDIR_SUFFIXES}
        )
        find_library(_ROKEN_LIBRARY
                    NAMES
                        ${_ROKEN_LIBNAME}
                    HINTS
                        ${_KRB5_LIBDIR_HINTS}
                    PATH_SUFFIXES
                        ${_KRB5_LIBDIR_SUFFIXES}
          )
        list(APPEND _KRB5_LIBRARIES ${_KRB5_LIBRARY})
      endif()
    endif()

    execute_process(
          COMMAND ${_KRB5_CONFIGURE_SCRIPT} "--version"
          OUTPUT_VARIABLE _KRB5_VERSION
          RESULT_VARIABLE _KRB5_CONFIGURE_FAILED
    )

    # older versions may not have the "--version" parameter. In this case we just don't care.
    if(_KRB5_CONFIGURE_FAILED)
      set(_KRB5_VERSION 0)
    endif()

  endif()
else()
  if(_KRB5_PKG_${_MIT_MODNAME}_VERSION)
    set(KRB5_FLAVOUR "MIT")
    set(_KRB5_VERSION _KRB5_PKG_${_MIT_MODNAME}_VERSION)
  else()
    set(KRB5_FLAVOUR "Heimdal")
    set(_KRB5_VERSION _KRB5_PKG_${_HEIMDAL_MODNAME}_VERSION)
  endif()
endif()

set(KRB5_INCLUDE_DIR ${_KRB5_INCLUDE_DIR})
set(KRB5_LIBRARIES ${_KRB5_LIBRARIES})
set(KRB5_LINK_DIRECTORIES ${_KRB5_LINK_DIRECTORIES})
set(KRB5_LINKER_FLAGS ${_KRB5_LINKER_FLAGS})
set(KRB5_COMPILER_FLAGS ${_KRB5_COMPILER_FLAGS})
set(KRB5_VERSION ${_KRB5_VERSION})

if(KRB5_FLAVOUR)
  if(NOT KRB5_VERSION AND KRB5_FLAVOUR STREQUAL "Heimdal")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
      set(HEIMDAL_MANIFEST_FILE "Heimdal.Application.amd64.manifest")
    else()
      set(HEIMDAL_MANIFEST_FILE "Heimdal.Application.x86.manifest")
    endif()
    if(EXISTS "${KRB5_INCLUDE_DIR}/${HEIMDAL_MANIFEST_FILE}")
      file(STRINGS "${KRB5_INCLUDE_DIR}/${HEIMDAL_MANIFEST_FILE}" heimdal_version_str
                 REGEX "^.*version=\"[0-9]\\.[^\"]+\".*$")

      string(REGEX MATCH "[0-9]\\.[^\"]+"
                   KRB5_VERSION "${heimdal_version_str}")
    endif()
    if(NOT KRB5_VERSION)
      set(KRB5_VERSION "Heimdal Unknown")
    endif()
  elseif(NOT KRB5_VERSION AND KRB5_FLAVOUR STREQUAL "MIT")
    get_filename_component(_MIT_VERSION "[HKEY_LOCAL_MACHINE\\SOFTWARE\\MIT\\Kerberos\\SDK\\CurrentVersion;VersionString]" NAME CACHE)
    if(WIN32 AND _MIT_VERSION)
      set(KRB5_VERSION "${_MIT_VERSION}")
    else()
      set(KRB5_VERSION "MIT Unknown")
    endif()
  endif()
endif()

include(FindPackageHandleStandardArgs)

set(_KRB5_REQUIRED_VARS KRB5_LIBRARIES KRB5_FLAVOUR)

find_package_handle_standard_args(KRB5
    REQUIRED_VARS
        ${_KRB5_REQUIRED_VARS}
    VERSION_VAR
        KRB5_VERSION
    FAIL_MESSAGE
        "Could NOT find Kerberos, try to set the path to Kerberos root folder in the system variable KRB_ROOT_DIR"
)

if(KRB5_FLAVOUR STREQUAL "MIT")
  string(STRIP "${KRB5_VERSION}" KRB5_VERSION)
  string(SUBSTRING ${KRB5_VERSION} 19 -1 KRB5_RELEASE_NUMBER)
  string(REGEX MATCH "([0-9]+)\\." KRB5_VERSION_MAJOR ${KRB5_RELEASE_NUMBER})
  string(REGEX REPLACE "\\." "" KRB5_VERSION_MAJOR "${KRB5_VERSION_MAJOR}")
  string(REGEX MATCH "\\.([0-9]+)$" KRB5_VERSION_MINOR ${KRB5_RELEASE_NUMBER})
  if(NOT KRB5_VERSION_MINOR)
    string(REGEX MATCH "\\.([0-9]+)[-\\.]" KRB5_VERSION_MINOR ${KRB5_RELEASE_NUMBER})
    string(REGEX REPLACE "\\." ""     KRB5_VERSION_MINOR "${KRB5_VERSION_MINOR}")
    string(REGEX REPLACE "\\." ""     KRB5_VERSION_MINOR "${KRB5_VERSION_MINOR}")
    string(REGEX REPLACE "-"   ""     KRB5_VERSION_MINOR "${KRB5_VERSION_MINOR}")
    string(REGEX MATCH "\\.([0-9]+)$" KRB5_VERSION_PATCH "${KRB5_RELEASE_NUMBER}")
    string(REGEX REPLACE "\\." ""     KRB5_VERSION_PATCH "${KRB5_VERSION_PATCH}")
    if(NOT KRB5_VERSION_PATCH)
      set(KRB5_VERSION_PATCH "0")
    endif()
  else()
    string(REGEX REPLACE "\\." ""     KRB5_VERSION_MINOR "${KRB5_VERSION_MINOR}")
    set(KRB5_VERSION_PATCH "0")
  endif()
  if(KRB5_VERSION_MAJOR AND KRB5_VERSION_MINOR)
    string(COMPARE GREATER ${KRB5_VERSION_MAJOR} 0 KRB5_VERSION_1)
    string(COMPARE GREATER ${KRB5_VERSION_MINOR} 12 KRB5_VERSION_1_13)
  else()
    message(SEND_ERROR "Failed to retrieved Kerberos version number")
  endif()
  message(STATUS "Located Kerberos ${KRB5_VERSION_MAJOR}.${KRB5_VERSION_MINOR}.${KRB5_VERSION_PATCH}")
endif()

mark_as_advanced(KRB5_INCLUDE_DIR KRB5_LIBRARIES)
