# - Try to find the GSS Kerberos libraries
# Once done this will define
#
#  GSS_ROOT_DIR - Set this variable to the root installation of GSS
#  GSS_ROOT_FLAVOUR - Set this variable to the flavour of Kerberos installation (MIT or Heimdal)
#
# Read-Only variables:
#  GSS_FOUND - system has the Heimdal library
#  GSS_FLAVOUR - "MIT" or "Heimdal" if anything found.
#  GSS_INCLUDE_DIR - the Heimdal include directory
#  GSS_LIBRARIES - The libraries needed to use GSS
#  GSS_LINK_DIRECTORIES - Directories to add to linker search path
#  GSS_LINKER_FLAGS - Additional linker flags
#  GSS_COMPILER_FLAGS - Additional compiler flags
#  GSS_VERSION - This is set to version advertised by pkg-config or read from manifest.
#                In case the library is found but no version info availabe it'll be set to "unknown"

set(_MIT_MODNAME mit-krb5-gssapi)
set(_HEIMDAL_MODNAME heimdal-gssapi)

include(CheckIncludeFile)
include(CheckIncludeFiles)
include(CheckTypeSize)

# export GSS_ROOT_FLAVOUR to use pkg-config system under UNIX
if(UNIX)
  if(NOT "$ENV{GSS_ROOT_FLAVOUR} " STREQUAL " ")
    string(REGEX MATCH "^[M|m]it$" MIT_FLAVOUR "$ENV{GSS_ROOT_FLAVOUR}")
    if(NOT MIT_FLAVOUR)
      string(REGEX MATCH "^MIT$" MIT_FLAVOUR "$ENV{GSS_ROOT_FLAVOUR}")
    endif()
    string(REGEX MATCH "^[H|h]eimdal$" HEIMDAL_FLAVOUR "$ENV{GSS_ROOT_FLAVOUR}")
    if(NOT HEIMDAL_FLAVOUR)
      string(REGEX MATCH "^HEIMDAL$" HEIMDAL_FLAVOUR "$ENV{GSS_ROOT_FLAVOUR}")
    endif()
    if(MIT_FLAVOUR)
      set(GSS_FLAVOUR "MIT")
    elseif(HEIMDAL_FLAVOUR)
      set(GSS_FLAVOUR "Heimdal")
    else()
      message(SEND_ERROR "Kerberos flavour unknown ($ENV{GSS_ROOT_FLAVOUR}). Choose MIT or Heimdal.")
    endif()
  endif()
endif()

set(_GSS_ROOT_HINTS
    "${GSS_ROOT_DIR}"
    "$ENV{GSS_ROOT_DIR}"
)

# try to find library using system pkg-config if user did not specify root dir
if(UNIX)
  if("$ENV{GSS_ROOT_DIR} " STREQUAL " ")
    if(GSS_FLAVOUR)
      find_package(PkgConfig QUIET)
      if(GSS_FLAVOUR STREQUAL "MIT")
        pkg_search_module(_GSS_PKG ${_MIT_MODNAME})
      else()
        pkg_search_module(_GSS_PKG ${_HEIMDAL_MODNAME})
      endif()

      if("${_GSS_PKG_PREFIX} " STREQUAL " ")
        if(NOT "$ENV{PKG_CONFIG_PATH} " STREQUAL " ")
          list(APPEND _GSS_ROOT_HINTS "$ENV{PKG_CONFIG_PATH}")
        else()
          message(SEND_ERROR "pkg_search_module failed : try to set PKG_CONFIG_PATH to PREFIX_OF_KERBEROS/lib/pkgconfig")
        endif()
      else()
	if($ENV{GSS_ROOT_FLAVOUR} STREQUAL "Heimdal")
	  string(FIND "${_GSS_PKG_PREFIX}" "heimdal" PKG_HEIMDAL_PREFIX_POSITION)
          if(PKG_HEIMDAL_PREFIX_POSITION STREQUAL "-1")
	    message(WARNING "Try to set PKG_CONFIG_PATH to \"PREFIX_OF_KERBEROS/lib/pkgconfig\"")
	  else()
	    list(APPEND _GSS_ROOT_HINTS "${_GSS_PKG_PREFIX}")
          endif()
	else()
	  list(APPEND _GSS_ROOT_HINTS "${_GSS_PKG_PREFIX}")
	endif()
      endif()
    else()
      message(WARNING "export GSS_ROOT_FLAVOUR to use pkg-config")
    endif()
  endif()
elseif(WIN32)
  list(APPEND _GSS_ROOT_HINTS "[HKEY_LOCAL_MACHINE\\SOFTWARE\\MIT\\Kerberos;InstallDir]")
endif()

if(NOT GSS_FOUND) # not found by pkg-config. Let's take more traditional approach.
  find_file(_GSS_CONFIGURE_SCRIPT
      NAMES
          "krb5-config"
      HINTS
          ${_GSS_ROOT_HINTS}
      PATH_SUFFIXES
          bin
      NO_CMAKE_PATH
      NO_CMAKE_ENVIRONMENT_PATH
  )

  # if not found in user-supplied directories, maybe system knows better
  find_file(_GSS_CONFIGURE_SCRIPT
      NAMES
          "krb5-config"
      PATH_SUFFIXES
          bin
  )

  execute_process(
       COMMAND ${_GSS_CONFIGURE_SCRIPT} "--vendor"
       OUTPUT_VARIABLE _GSS_VENDOR
       RESULT_VARIABLE _GSS_CONFIGURE_FAILED
  )

  if(NOT _GSS_CONFIGURE_FAILED)
    string(STRIP "${_GSS_VENDOR}" _GSS_VENDOR)
    if((GSS_FLAVOUR STREQUAL "Heimdal" AND NOT _GSS_VENDOR STREQUAL "Heimdal")
       OR (GSS_FLAVOUR STREQUAL "MIT" AND NOT _GSS_VENDOR STREQUAL "Massachusetts Institute of Technology"))
      message(SEND_ERROR "GSS vendor and GSS flavour are not matching : _GSS_VENDOR=${_GSS_VENDOR} ; GSS_FLAVOUR=${GSS_FLAVOUR}") 
      message(STATUS "Try to set the path to GSS root folder in the system variable GSS_ROOT_DIR")
    endif()
  else()
    message(SEND_ERROR "GSS configure script failed to get vendor")
  endif()

  # NOTE: fail to link Heimdal libraries using configure script due to limitations
  # during Heimdal linking process. Then, we do it "manually".
  if(NOT "${_GSS_CONFIGURE_SCRIPT} " STREQUAL " " AND GSS_FLAVOUR AND NOT _GSS_VENDOR STREQUAL "Heimdal")
    execute_process(
          COMMAND ${_GSS_CONFIGURE_SCRIPT} "--cflags" "gssapi"
          OUTPUT_VARIABLE _GSS_CFLAGS
          RESULT_VARIABLE _GSS_CONFIGURE_FAILED
    )

    if(NOT _GSS_CONFIGURE_FAILED) # 0 means success
      # should also work in an odd case when multiple directories are given
      string(STRIP "${_GSS_CFLAGS}" _GSS_CFLAGS)
      string(REGEX REPLACE " +-I" ";" _GSS_CFLAGS "${_GSS_CFLAGS}")
      string(REGEX REPLACE " +-([^I][^ \\t;]*)" ";-\\1" _GSS_CFLAGS "${_GSS_CFLAGS}")

      foreach(_flag ${_GSS_CFLAGS})
        if(_flag MATCHES "^-I.*")
          string(REGEX REPLACE "^-I" "" _val "${_flag}")
          list(APPEND _GSS_INCLUDE_DIR "${_val}")
        else()
          list(APPEND _GSS_COMPILER_FLAGS "${_flag}")
        endif()
      endforeach()
    endif()

    if(_GSS_VENDOR STREQUAL "Massachusetts Institute of Technology")
      execute_process(
            COMMAND ${_GSS_CONFIGURE_SCRIPT} "--libs" "gssapi"
            OUTPUT_VARIABLE _GSS_LIB_FLAGS
            RESULT_VARIABLE _GSS_CONFIGURE_FAILED
      )
    elseif(_GSS_VENDOR STREQUAL "Heimdal")
      execute_process(
            COMMAND ${_GSS_CONFIGURE_SCRIPT} "--deps --libs" "gssapi kafs"
            OUTPUT_VARIABLE _GSS_LIB_FLAGS
            RESULT_VARIABLE _GSS_CONFIGURE_FAILED
      )
    else()
      message(SEND_ERROR "Unknown vendor")
    endif()

    if(NOT _GSS_CONFIGURE_FAILED) # 0 means success
      # this script gives us libraries and link directories. We have to deal with it.
      string(STRIP "${_GSS_LIB_FLAGS}" _GSS_LIB_FLAGS)
      string(REGEX REPLACE " +-(L|l)" ";-\\1" _GSS_LIB_FLAGS "${_GSS_LIB_FLAGS}")
      string(REGEX REPLACE " +-([^Ll][^ \\t;]*)" ";-\\1" _GSS_LIB_FLAGS "${_GSS_LIB_FLAGS}")

      foreach(_flag ${_GSS_LIB_FLAGS})
        if(_flag MATCHES "^-l.*")
          string(REGEX REPLACE "^-l" "" _val "${_flag}")
          list(APPEND _GSS_LIBRARIES "${_val}")
        elseif(_flag MATCHES "^-L.*")
          string(REGEX REPLACE "^-L" "" _val "${_flag}")
          list(APPEND _GSS_LINK_DIRECTORIES "${_val}")
        else()
          list(APPEND _GSS_LINKER_FLAGS "${_flag}")
        endif()
      endforeach()

    endif()

    execute_process(
          COMMAND ${_GSS_CONFIGURE_SCRIPT} "--version"
          OUTPUT_VARIABLE _GSS_VERSION
          RESULT_VARIABLE _GSS_CONFIGURE_FAILED
    )

    # older versions may not have the "--version" parameter. In this case we just don't care.
    if(_GSS_CONFIGURE_FAILED)
      set(_GSS_VERSION 0)
    endif()

  else() # either there is no config script or we are on platform that doesn't provide one (Windows?)
    if(_GSS_VENDOR STREQUAL "Massachusetts Institute of Technology")
      find_path(_GSS_INCLUDE_DIR
               NAMES
                   "gssapi/gssapi_generic.h"
               HINTS
                   ${_GSS_ROOT_HINTS}
               PATH_SUFFIXES
                   include
                   inc
      )

      if(_GSS_INCLUDE_DIR) # we've found something
        set(CMAKE_REQUIRED_INCLUDES "${_GSS_INCLUDE_DIR}")
        check_include_files( "gssapi/gssapi_generic.h;gssapi/gssapi_ext.h" _GSS_HAVE_MIT_HEADERS)
        if(_GSS_HAVE_MIT_HEADERS)
          set(GSS_FLAVOUR "MIT")
        else()
          message(SEND_ERROR "Try to set the Kerberos flavour (GSS_ROOT_FLAVOUR)")
        endif()
      elseif("$ENV{PKG_CONFIG_PATH} " STREQUAL " ")
	message(WARNING "Try to set PKG_CONFIG_PATH to PREFIX_OF_KERBEROS/lib/pkgconfig")
      endif()
    elseif(_GSS_VENDOR STREQUAL "Heimdal")
      find_path(_GSS_INCLUDE_DIR
               NAMES
                   "gssapi/gssapi_spnego.h"
               HINTS
                   ${_GSS_ROOT_HINTS}
	       PATHS
	           /usr/heimdal
		   /usr/local/heimdal
               PATH_SUFFIXES
                   include
                   inc
      )

      if(_GSS_INCLUDE_DIR) # we've found something
        set(CMAKE_REQUIRED_INCLUDES "${_GSS_INCLUDE_DIR}")
        # prevent compiling the header - just check if we can include it
        set(CMAKE_REQUIRED_DEFINITIONS "${CMAKE_REQUIRED_DEFINITIONS} -D__ROKEN_H__")
        check_include_file( "roken.h" _GSS_HAVE_ROKEN_H)
        check_include_file( "heimdal/roken.h" _GSS_HAVE_HEIMDAL_ROKEN_H)
        if(_GSS_HAVE_ROKEN_H OR _GSS_HAVE_HEIMDAL_ROKEN_H)
          set(GSS_FLAVOUR "Heimdal")
        endif()
        set(CMAKE_REQUIRED_DEFINITIONS "")
      elseif("$ENV{PKG_CONFIG_PATH} " STREQUAL " ")
        message(WARNING "Try to set PKG_CONFIG_PATH to PREFIX_OF_KERBEROS/lib/pkgconfig")
      endif()
    else()
      message(SEND_ERROR "Kerberos vendor unknown (${_GSS_VENDOR})")
    endif()

    # if we have headers, check if we can link libraries
    if(GSS_FLAVOUR)
      set(_GSS_LIBDIR_SUFFIXES "")
      set(_GSS_LIBDIR_HINTS ${_GSS_ROOT_HINTS})
      get_filename_component(_GSS_CALCULATED_POTENTIAL_ROOT "${_GSS_INCLUDE_DIR}" PATH)
      list(APPEND _GSS_LIBDIR_HINTS ${_GSS_CALCULATED_POTENTIAL_ROOT})

      if(WIN32)
        if(CMAKE_SIZEOF_VOID_P EQUAL 8)
          list(APPEND _GSS_LIBDIR_SUFFIXES "lib/AMD64")
          if(GSS_FLAVOUR STREQUAL "MIT")
            set(_GSS_LIBNAME "gssapi64")
          else()
            set(_GSS_LIBNAME "libgssapi")
          endif()
        else()
          list(APPEND _GSS_LIBDIR_SUFFIXES "lib/i386")
          if(GSS_FLAVOUR STREQUAL "MIT")
            set(_GSS_LIBNAME "gssapi32")
          else()
            set(_GSS_LIBNAME "libgssapi")
          endif()
        endif()
      else()
        list(APPEND _GSS_LIBDIR_SUFFIXES "lib;lib64;x86_64-linux-gnu") # those suffixes are not checked for HINTS
        if(GSS_FLAVOUR STREQUAL "MIT")
          set(_GSS_LIBNAME "gssapi_krb5")
          set(_KRB5_LIBNAME "krb5")
          set(_COMERR_LIBNAME "com_err")
          set(_KRB5SUPPORT_LIBNAME "krb5support")
        else()
          set(_GSS_LIBNAME "gssapi")
          set(_KRB5_LIBNAME "krb5")
          set(_KAFS_LIBNAME "kafs")
          set(_ROKEN_LIBNAME "roken")
        endif()
      endif()

      find_library(_GSS_LIBRARIES
                  NAMES
                      ${_GSS_LIBNAME}
                  HINTS
                      ${_GSS_LIBDIR_HINTS}
                  PATH_SUFFIXES
                      ${_GSS_LIBDIR_SUFFIXES}
      )

      if(GSS_FLAVOUR STREQUAL "MIT")
        find_library(_KRB5_LIBRARY
                    NAMES
                        ${_KRB5_LIBNAME}
                    HINTS
                        ${_GSS_LIBDIR_HINTS}
                    PATH_SUFFIXES
                        ${_GSS_LIBDIR_SUFFIXES}
        )
        find_library(_COMERR_LIBRARY
                    NAMES
                        ${_COMERR_LIBNAME}
                    HINTS
                        ${_GSS_LIBDIR_HINTS}
                    PATH_SUFFIXES
                        ${_GSS_LIBDIR_SUFFIXES}
        )
        find_library(_KRB5SUPPORT_LIBRARY
                    NAMES
                        ${_KRB5SUPPORT_LIBNAME}
                    HINTS
                        ${_GSS_LIBDIR_HINTS}
                    PATH_SUFFIXES
                        ${_GSS_LIBDIR_SUFFIXES}
        )
        list(APPEND _GSS_LIBRARIES ${_KRB5_LIBRARY} ${_KRB5SUPPORT_LIBRARY} ${_COMERR_LIBRARY})
      endif()

      if(GSS_FLAVOUR STREQUAL "Heimdal")
        find_library(_KRB5_LIBRARY
	            NAMES
                        ${_KRB5_LIBNAME}
                    HINTS
                        ${_GSS_LIBDIR_HINTS}
                    PATH_SUFFIXES
                        ${_GSS_LIBDIR_SUFFIXES}
        )
	find_library(_KAFS_LIBRARY
		    NAMES
                        ${_KAFS_LIBNAME}
                    HINTS
                        ${_GSS_LIBDIR_HINTS}
                    PATH_SUFFIXES
                        ${_GSS_LIBDIR_SUFFIXES}
        )
	find_library(_ROKEN_LIBRARY
		    NAMES
                        ${_ROKEN_LIBNAME}
                    HINTS
                        ${_GSS_LIBDIR_HINTS}
                    PATH_SUFFIXES
                        ${_GSS_LIBDIR_SUFFIXES}
          )
	list(APPEND _GSS_LIBRARIES ${_KRB5_LIBRARY} ${_KAFS_LIBRARY} ${_ROKEN_LIBRARY})
      endif()
    endif()

    execute_process(
          COMMAND ${_GSS_CONFIGURE_SCRIPT} "--version"
          OUTPUT_VARIABLE _GSS_VERSION
          RESULT_VARIABLE _GSS_CONFIGURE_FAILED
    )

    # older versions may not have the "--version" parameter. In this case we just don't care.
    if(_GSS_CONFIGURE_FAILED)
      set(_GSS_VERSION 0)
    endif()

  endif()
else()
  if(_GSS_PKG_${_MIT_MODNAME}_VERSION)
    set(GSS_FLAVOUR "MIT")
    set(_GSS_VERSION _GSS_PKG_${_MIT_MODNAME}_VERSION)
  else()
    set(GSS_FLAVOUR "Heimdal")
    set(_GSS_VERSION _GSS_PKG_${_HEIMDAL_MODNAME}_VERSION)
  endif()
endif()

set(GSS_INCLUDE_DIR ${_GSS_INCLUDE_DIR})
set(GSS_LIBRARIES ${_GSS_LIBRARIES})
set(GSS_LINK_DIRECTORIES ${_GSS_LINK_DIRECTORIES})
set(GSS_LINKER_FLAGS ${_GSS_LINKER_FLAGS})
set(GSS_COMPILER_FLAGS ${_GSS_COMPILER_FLAGS})
set(GSS_VERSION ${_GSS_VERSION})

if(GSS_FLAVOUR)
  if(NOT GSS_VERSION AND GSS_FLAVOUR STREQUAL "Heimdal")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
      set(HEIMDAL_MANIFEST_FILE "Heimdal.Application.amd64.manifest")
    else()
      set(HEIMDAL_MANIFEST_FILE "Heimdal.Application.x86.manifest")
    endif()
    if(EXISTS "${GSS_INCLUDE_DIR}/${HEIMDAL_MANIFEST_FILE}")
      file(STRINGS "${GSS_INCLUDE_DIR}/${HEIMDAL_MANIFEST_FILE}" heimdal_version_str
                 REGEX "^.*version=\"[0-9]\\.[^\"]+\".*$")

      string(REGEX MATCH "[0-9]\\.[^\"]+"
                   GSS_VERSION "${heimdal_version_str}")
    endif()
    if(NOT GSS_VERSION)
      set(GSS_VERSION "Heimdal Unknown")
    endif()
  elseif(NOT GSS_VERSION AND GSS_FLAVOUR STREQUAL "MIT")
    get_filename_component(_MIT_VERSION "[HKEY_LOCAL_MACHINE\\SOFTWARE\\MIT\\Kerberos\\SDK\\CurrentVersion;VersionString]" NAME CACHE)
    if(WIN32 AND _MIT_VERSION)
      set(GSS_VERSION "${_MIT_VERSION}")
    else()
      set(GSS_VERSION "MIT Unknown")
    endif()
  endif()
endif()

include(FindPackageHandleStandardArgs)

set(_GSS_REQUIRED_VARS GSS_LIBRARIES GSS_FLAVOUR)

find_package_handle_standard_args(GSS
    REQUIRED_VARS
        ${_GSS_REQUIRED_VARS}
    VERSION_VAR
        GSS_VERSION
    FAIL_MESSAGE
        "Could NOT find GSS, try to set the path to GSS root folder in the system variable GSS_ROOT_DIR"
)

if(GSS_FLAVOUR STREQUAL "MIT")
  string(STRIP "${GSS_VERSION}" GSS_VERSION)
  string(SUBSTRING ${GSS_VERSION} 19 -1 GSS_RELEASE_NUMBER)
  string(REGEX MATCH "([0-9]+)\\." GSS_VERSION_MAJOR ${GSS_RELEASE_NUMBER})
  string(REGEX REPLACE "\\." "" GSS_VERSION_MAJOR "${GSS_VERSION_MAJOR}")
  string(REGEX MATCH "\\.([0-9]+)$" GSS_VERSION_MINOR ${GSS_RELEASE_NUMBER})
  if(NOT GSS_VERSION_MINOR)
    string(REGEX MATCH "\\.([0-9]+)\\." GSS_VERSION_MINOR ${GSS_RELEASE_NUMBER})
    string(REGEX REPLACE "\\." "" GSS_VERSION_MINOR "${GSS_VERSION_MINOR}")
    string(REGEX REPLACE "\\." "" GSS_VERSION_MINOR "${GSS_VERSION_MINOR}")
    string(REGEX MATCH "([0-9]+)$" GSS_VERSION_PATCH ${GSS_RELEASE_NUMBER})
    string(REGEX REPLACE "\\." "" GSS_VERSION_PATCH "${GSS_VERSION_PATCH}")
  else()
    string(REGEX REPLACE "\\." "" GSS_VERSION_MINOR "${GSS_VERSION_MINOR}")
    set(GSS_VERSION_PATCH "0")
  endif()
  if(GSS_VERSION_MAJOR AND GSS_VERSION_MINOR)
    string(COMPARE GREATER ${GSS_VERSION_MAJOR} 0 GSS_VERSION_1)
    string(COMPARE GREATER ${GSS_VERSION_MINOR} 12 GSS_VERSION_1_13)
  else()
    message(SEND_ERROR "Failed to retrieved Kerberos version number")
  endif()
endif()

mark_as_advanced(GSS_INCLUDE_DIR GSS_LIBRARIES)
