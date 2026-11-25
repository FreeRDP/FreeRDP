# Helpers to recreate the imported targets a static library links against.
#
# A static library exports its private link dependencies (json-c::json-c,
# PkgConfig::opus, ...) as part of its target, so the generated <Package>Config.cmake
# has to find them again before including <Package>Targets.cmake. The lookups are
# taken from the libraries the target actually links (its link properties).
#
# Shared libraries keep these dependencies private, so nothing is needed for them.

# Write the lookups for the imported targets (or aliases of them) that <target> links
# against into <outvar>, as CMake code for a config file template. Call it after the
# last target_link_libraries() of <target>, in the directory that links it: imported
# targets are only visible in the directory that found them and below it.
#
# The libraries come from the target properties LINK_LIBRARIES and
# INTERFACE_LINK_LIBRARIES; the latter is what the exported target carries, with the
# private dependencies of a static library as $<LINK_ONLY:...>.
# PkgConfig::<prefix> is looked up again with pkg-config, any other <Package>::<name>
# with find_dependency(<Package>). Set the target property PACKAGE_CONFIG_NAME when
# the package is named differently than the namespace of its targets.
function(package_config_dependencies target outvar)
  set(_code "")
  if(BUILD_SHARED_LIBS)
    set(${outvar} "${_code}" PARENT_SCOPE)
    return()
  endif()

  get_target_property(_link "${target}" LINK_LIBRARIES)
  get_target_property(_interface "${target}" INTERFACE_LINK_LIBRARIES)
  set(_deps "")
  foreach(_lib IN LISTS _link _interface)
    if(_lib MATCHES "^\\$<LINK_ONLY:(.+)>$")
      set(_lib "${CMAKE_MATCH_1}")
    endif()
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
  list(REMOVE_DUPLICATES _deps)

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
