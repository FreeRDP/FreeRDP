# Helpers to recreate the imported targets a static library links against.
#
# A static library exports its private link dependencies (json-c::json-c,
# PkgConfig::opus, ...) as part of its target, so the generated <Package>Config.cmake
# has to find them again before including <Package>Targets.cmake. The imported
# targets are recorded where the library is linked, and the config file template
# expands the collected lookups.
#
# Shared libraries keep these dependencies private, so nothing is recorded for them.

# Record the imported targets (or aliases of them) among the link libraries of
# <var>'s library. PkgConfig::<prefix> is looked up again with pkg-config, any other <Package>::<name>
# with find_dependency(<Package>). Set the target property PACKAGE_CONFIG_NAME when
# the package is named differently than the namespace of its targets.
function(package_config_add_targets var)
  if(BUILD_SHARED_LIBS)
    return()
  endif()

  set(_deps ${${var}})
  foreach(_lib IN LISTS ARGN)
    if(NOT TARGET "${_lib}")
      continue()
    endif()
    get_target_property(_aliased "${_lib}" ALIASED_TARGET)
    if(_aliased)
      set(_lib "${_aliased}")
    endif()
    if(NOT _lib MATCHES "^([^:]+)::(.+)$")
      continue()
    endif()
    set(_namespace "${CMAKE_MATCH_1}")
    set(_name "${CMAKE_MATCH_2}")
    get_target_property(_imported "${_lib}" IMPORTED)
    if(NOT _imported)
      continue()
    endif()

    if(_namespace STREQUAL "PkgConfig")
      # set by pkg_check_modules for the module that was found
      set(_module "${${_name}_MODULE_NAME}")
      if(NOT _module)
        message(FATAL_ERROR "no pkg-config module recorded for ${_lib}")
      endif()
      list(APPEND _deps "pkgconfig:${_name}:${_module}")
    else()
      get_target_property(_package "${_lib}" PACKAGE_CONFIG_NAME)
      if(NOT _package)
        set(_package "${_namespace}")
      endif()
      list(APPEND _deps "find_dependency(${_package})")
    endif()
  endforeach()
  set(${var} ${_deps} CACHE INTERNAL "dependencies")
endfunction()

# Expand the recorded lookups of <var> into CMake code for a config file template
function(package_config_dependencies var outvar)
  set(_deps ${${var}})
  list(REMOVE_DUPLICATES _deps)

  set(_code "")
  set(_have_pkgconfig OFF)
  foreach(_dep IN LISTS _deps)
    if(_dep MATCHES "^pkgconfig:([^:]+):(.+)$")
      set(_prefix "${CMAKE_MATCH_1}")
      set(_module "${CMAKE_MATCH_2}")
      if(NOT _have_pkgconfig)
        string(APPEND _code "find_dependency(PkgConfig)\n")
        set(_have_pkgconfig ON)
      endif()
      string(
        APPEND
        _code
        "pkg_check_modules(${_prefix} QUIET IMPORTED_TARGET ${_module})\n"
        "if(NOT ${_prefix}_FOUND)\n"
        "\tset(\${CMAKE_FIND_PACKAGE_NAME}_NOT_FOUND_MESSAGE \"pkg-config module ${_module} not found\")\n"
        "\tset(\${CMAKE_FIND_PACKAGE_NAME}_FOUND FALSE)\n"
        "\treturn()\n"
        "endif()\n"
      )
    else()
      string(APPEND _code "${_dep}\n")
    endif()
  endforeach()
  set(${outvar} "${_code}" PARENT_SCOPE)
endfunction()
