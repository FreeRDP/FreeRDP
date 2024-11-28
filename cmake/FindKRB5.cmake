# - Try to find the Kerberos libraries
# Once done this will define
#
#  KRB5_ROOT_CONFIG - Set this variable to the full path of krb5-config of Kerberos
#  KRB5_ROOT_FLAVOUR - Set this variable to the flavour of Kerberos installation (MIT or Heimdal)
#
# Read-Only variables:
#  KRB5_FOUND - system has the Heimdal library
#  KRB5_FLAVOUR - "MIT" or "Heimdal" if anything found.
#  KRB5_INCLUDE_DIRS - the Heimdal include directory
#  KRB5_LIBRARIES - The libraries needed to use Kerberos
#  KRB5_LIBRARY_DIRS - Directories to add to linker search path
#  KRB5_LDFLAGS - Additional linker flags
#  KRB5_CFLAGS - Additional compiler flags
#  KRB5_VERSION - This is set to version advertised by pkg-config or read from manifest.
#                In case the library is found but no version info available it'll be set to "unknown"

include(CheckIncludeFile)
include(CheckIncludeFiles)
include(CheckTypeSize)

set(_KRB5_REQUIRED_VARS KRB5_FOUND KRB5_VERSION KRB5_FLAVOUR KRB5_INCLUDE_DIRS KRB5_LIBRARIES)

macro(PROVIDES_KRB5)
  set(PREFIX "MACRO_KRB5")

  cmake_parse_arguments("${PREFIX}" "" "NAME" "" ${ARGN})

  set(KRB5_FLAVOUR ${MACRO_KRB5_NAME})
  string(TOUPPER "${MACRO_KRB5_NAME}" MACRO_KRB5_NAME)

  # This is a list of all variables that pkg_check_modules exports.
  set(VARIABLES
      "CFLAGS;CFLAGS_I;CFLAGS_OTHER;FOUND;INCLUDEDIR;INCLUDE_DIRS;LDFLAGS;LDFLAGS_OTHER;LIBDIR;LIBRARIES;LIBRARY_DIRS;LIBS;LIBS_L;LIBS_OTHER;LIBS_PATHS;LINK_LIBRARIS;MODULE_NAME;PREFIX;VERSION;STATIC_CFLAGS;STATIC_CFLAGS_I;STATIC_CFLAGS_OTHER;STATIC_INCLUDE_DIRS;STATIC_LDFLAGS;STATIC_LDFLAGS_OTHER;STATIC_LIBDIR;STATIC_LIBRARIES;STATIC_LIBRARY_DIRS;STATIC_LIBS;STATIC_LIBS_L;STATIC_LIBS_OTHER;STATIC_LIBS_PATHS"
  )
  foreach(VAR ${VARIABLES})
    set(KRB5_${VAR} ${KRB5_${MACRO_KRB5_NAME}_${VAR}})
  endforeach()

  # Bugfix for older installations:
  # KRB5_INCLUDE_DIRS might not be set, fall back to KRB5_INCLUDEDIR
  if(NOT KRB5_INCLUDE_DIRS)
    set(KRB5_INCLUDE_DIRS ${KRB5_INCLUDEDIR})
  endif()
endmacro()

function(GET_KRB5_CONFIG KRB5_CONFIG COMMAND RESULT)
  execute_process(COMMAND ${KRB5_CONFIG} ${COMMAND} OUTPUT_VARIABLE _KRB5_RESULT RESULT_VARIABLE _KRB5_CONFIGURE_FAILED)
  if(_KRB5_CONFIGURE_FAILED)
    message(FATAL_ERROR "Failed to detect krb5-config [${COMMAND}]")
  endif()

  string(REGEX REPLACE "[\r\n]" "" _KRB5_RESULT ${_KRB5_RESULT})
  set(${RESULT} "${_KRB5_RESULT}" PARENT_SCOPE)
endfunction()

function(string_starts_with str search RES)
  string(FIND "${str}" "${search}" out)
  if("${out}" EQUAL 0)
    set(${RES} ON PARENT_SCOPE)
  else()
    set(${RES} OFF PARENT_SCOPE)
  endif()
endfunction()

function(GET_KRB5_BY_CONFIG KRB5_CONFIG)
  if(NOT KRB5_CONFIG)
    find_program(
      KRB5_CONFIG NAMES "krb5-config" "krb5-config.mit" "krb5-config.heimdal" PATH_SUFFIXES bin
      NO_CMAKE_PATH NO_CMAKE_ENVIRONMENT_PATH REQUIRED
    )
    message("autodetected krb5-config at ${KRB5_CONFIG}")
    if(NOT KRB5_CONFIG)
      return()
    endif()
  else()
    message("using krb5-config ${KRB5_CONFIG} provided by KRB5_ROOT_CONFIG")
  endif()

  get_krb5_config("${KRB5_CONFIG}" "--vendor" _KRB5_VENDOR)

  if("${_KRB5_VENDOR}" STREQUAL "Apple MITKerberosShim")
    message(FATAL_ERROR "Apple MITKerberosShim is deprecated and not supported")
  elseif("${_KRB5_VENDOR}" STREQUAL "Massachusetts Institute of Technology")
    set(KRB5_FLAVOUR "MIT")
  else()
    set(KRB5_FLAVOUR "${_KRB5_VENDOR}")
  endif()

  get_krb5_config("${KRB5_CONFIG}" "--cflags" KRB5_CFLAGS)
  get_krb5_config("${KRB5_CONFIG}" "--version" KRB5_VERSION_RAW)

  string(REGEX REPLACE "[ ]" ";" KRB5_VERSION_LIST "${KRB5_VERSION_RAW}")
  list(LENGTH KRB5_VERSION_LIST KRB5_VERSION_LIST_LEN)
  math(EXPR KRB5_VERSION_LIST_LAST "${KRB5_VERSION_LIST_LEN} - 1")
  list(GET KRB5_VERSION_LIST ${KRB5_VERSION_LIST_LAST} KRB5_VERSION)

  if(KRB5_FLAVOUR STREQUAL "MIT")
    if(KRB5_VERSION VERSION_LESS "1.14")
      message(FATAL_ERROR "MIT kerberos ${KRB5_VERSION} < 1.14 is not supported, upgrade the library!")
    endif()
  endif()

  get_krb5_config("${KRB5_CONFIG}" "--libs" KRB5_LDFLAGS)

  string(REGEX REPLACE "[ ]" ";" KRB5_CFLAG_LIST "${KRB5_CFLAGS}")
  foreach(FLAG ${KRB5_CFLAG_LIST})
    string_starts_with("${FLAG}" "-I" RES)
    if(RES)
      string(SUBSTRING "${FLAG}" 2 -1 FLAG)
    endif()

    if(IS_DIRECTORY "${FLAG}")
      list(APPEND KRB5_INCLUDEDIR ${FLAG})
    endif()
  endforeach()
  if(NOT KRB5_INCLUDEDIR)
    find_file(KRB5_INCLUDEDIR_HEADER NAMES krb5.h REQUIRED)
    get_filename_component(KRB5_INCLUDEDIR "${KRB5_INCLUDEDIR_HEADER}" DIRECTORY)
  endif()

  string(REGEX REPLACE "[ ]" ";" KRB5_LDFLAG_LIST "${KRB5_LDFLAGS}")
  foreach(FLAG ${KRB5_LDFLAG_LIST})
    string_starts_with("${FLAG}" "-L" RES)
    if(RES)
      string(SUBSTRING "${FLAG}" 2 -1 SUBFLAG)
      list(APPEND KRB5_LIBRARY_DIRS ${SUBFLAG})
    endif()
    string_starts_with("${FLAG}" "-l" RES)
    if(RES)
      string(SUBSTRING "${FLAG}" 2 -1 SUBFLAG)
      list(APPEND KRB5_LIBRARIES ${SUBFLAG})
    endif()
  endforeach()

  set(KRB5_FOUND ON PARENT_SCOPE)
  set(KRB5_VERSION ${KRB5_VERSION} PARENT_SCOPE)
  set(KRB5_FLAVOUR ${KRB5_FLAVOUR} PARENT_SCOPE)
  set(KRB5_CFLAGS ${KRB5_CFLAGS} PARENT_SCOPE)
  set(KRB5_LDFLAGS ${KRB5_LDFLAGS} PARENT_SCOPE)
  set(KRB5_INCLUDEDIR ${KRB5_INCLUDEDIR} PARENT_SCOPE)
  set(KRB5_INCLUDE_DIRS ${KRB5_INCLUDEDIR} PARENT_SCOPE)
  set(KRB5_LIBRARIES ${KRB5_LIBRARIES} PARENT_SCOPE)
  set(KRB5_LIBRARY_DIRS ${KRB5_LIBRARY_DIRS} PARENT_SCOPE)
endfunction()

# try to find kerberos to compile against.
#
# * First search with pkg-config (prefer MIT over Heimdal)
# * Then try to find krb5-config (generic, krb5-config.mit and last krb5-config.heimdal)
find_package(PkgConfig REQUIRED)

if(KRB5_ROOT_CONFIG)

elseif(KRB5_ROOT_FLAVOUR)
  if(KRB5_ROOT_FLAVOUR STREQUAL "Heimdal")
    pkg_check_modules(KRB5_HEIMDAL heimdal-krb5)
  elseif(KRB5_ROOT_FLAVOUR STREQUAL "MIT")
    pkg_check_modules(KRB5_HEIMDAL mit-krb5)
  else()
    message(FATAL_ERROR "Invalid KRB5_ROOT_FLAVOUR=${KRB5_ROOT_FLAVOUR}, only 'MIT' or 'Heimdal' are supported")
  endif()
else()
  pkg_check_modules(KRB5_MIT mit-krb5)
  pkg_check_modules(KRB5_HEIMDAL heimdal-krb5)
endif()

if(KRB5_MIT_FOUND)
  provides_krb5(NAME "MIT")
  if(KRB5_VERSION VERSION_LESS "1.14")
    message(FATAL_ERROR "MIT kerberos < 1.14 is not supported, upgrade the library!")
  endif()
elseif(KRB5_HEIMDAL_FOUND)
  provides_krb5(NAME "Heimdal")
elseif(KRB5_ANY_FOUND)
  get_krb5_vendor(ANY_VENDOR)
  provides_krb5(NAME "${ANY_VENDOR}")
else()
  get_krb5_by_config("${KRB5_ROOT_CONFIG}")
endif()

message("using KRB5_FOUND ${KRB5_FOUND} ")
message("using KRB5_VERSION ${KRB5_VERSION} ")
message("using KRB5_FLAVOUR ${KRB5_FLAVOUR} ")
message("using KRB5_CFLAGS ${KRB5_CFLAGS} ")
message("using KRB5_LDFLAGS ${KRB5_LDFLAGS} ")
message("using KRB5_INCLUDEDIR ${KRB5_INCLUDEDIR} ")
message("using KRB5_INCLUDE_DIRS ${KRB5_INCLUDE_DIRS} ")
message("using KRB5_LIBRARIES ${KRB5_LIBRARIES} ")

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(
  KRB5 REQUIRED_VARS ${_KRB5_REQUIRED_VARS} VERSION_VAR KRB5_VERSION
  FAIL_MESSAGE "Could NOT find Kerberos, try to set the CMake variable KRB5_ROOT_CONFIG to the full path of krb5-config"
)

mark_as_advanced(${_KRB5_REQUIRED_VARS})
