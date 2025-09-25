function(detect_package name)
  set(options REQUIRED)
  set(oneValueArgs HEADER LIBRARY PKGCONFIG CMAKE)
  cmake_parse_arguments(PARSE_ARGV 0 arg "${options}" "${oneValueArgs}" "${multiValueArgs}")

  find_package(${name})
  if(NOT ${name}_FOUND)
    # Fallback detection:
    # some distributions only ship pkg-config and skip CMake config files.
    find_package(PkgConfig)
    if(PKG_CONFIG_FOUND)
      if(arg_PKGCONFIG)
        set(module ${arg_PKGCONFIG})
      else()
        set(module ${name})
      endif()
      pkg_check_modules(${name} ${module})
    endif()

    # No CMake nor pkg-config files available, try manual detection
    if(NOT ${name}_FOUND)
      find_library(
        ${name}_LIBRARY NAMES ${arg_LIBRARY} ${arg_PKGCONFIG} ${arg_CMAKE} ${name} ${arg_CMAKE}
        PATH_SUFFIXES ${name} ${arg_PKGCONFIG} ${arg_CMAKE}
      )

      find_path(${name}_HEADER NAMES ${arg_HEADER} ${name}.h ${arg_PKGCONFIG}.h ${arg_CMAKE}.h
                PATH_SUFFIXES ${name} ${arg_PKGCONFIG} ${arg_CMAKE}
      )

      if(NOT ${name}_LIBRARY STREQUAL "${name}_LIBRARY-NOTFOUND" AND NOT ${name}_HEADER STREQUAL
                                                                     "${name}_HEADER-NOTFOUND"
      )
        set(${name}_LINK_LIBRARIES "${${name}_LIBRARY}")
        set(${name}_INCLUDE_DIRS "${${name}_HEADER}")
        get_filename_component(${name}_LIB_FILE "${${name}_LIBRARY}" NAME_WE)
        set(regex "^${CMAKE_SHARED_LIBRARY_SUFFIX}")
        string(REGEX REPLACE ${regex} ${name}_LIBRARIES "${${name}_LIB_FILE}")
      endif()
    endif()

    if(${name}_FOUND)
      # Create imported target cjson
      add_library(${arg_CMAKE} SHARED IMPORTED)
      set_property(TARGET ${arg_CMAKE} APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
      set_target_properties(
        ${arg_CMAKE}
        PROPERTIES IMPORTED_CONFIGURATIONS NOCONFIG IMPORTED_LINK_INTERFACE_LANGUAGES_NOCONFIG "C"
                   IMPORTED_LOCATION_NOCONFIG "${${name}_LINK_LIBRARIES}" IMPORTED_SONAME_NOCONFIG
                                                                          "${${name}_LIBRARIES}"
                   INTERFACE_INCLUDE_DIRECTORIES "${${name}_INCLUDE_DIRS}"
      )
    endif()
  endif()

  if(NOT ${name}_FOUND AND arg_REQUIRED)
    message(FATAL_ERROR "${name} was REQUIRED but not found")
  endif()

  if(${${name}_FOUND})
    set(${name}_FOUND ${${name}_FOUND} CACHE INTERNAL "internal")
  else()
    unset(${name}_FOUND CACHE)
  endif()
endfunction()

include(CMakeDependentOption)

option(WITH_JSON_DISABLED "Build without any JSON support" OFF)
cmake_dependent_option(
  WITH_CJSON_REQUIRED "Build with cJSON (fail if not found)" OFF
  "NOT WITH_JSON_DISABLED;NOT WITH_JSONC_REQUIRED;NOT WITH_JANSSON_REQUIRED" OFF
)
cmake_dependent_option(
  WITH_JSONC_REQUIRED "Build with JSON-C (fail if not found)" OFF
  "NOT WITH_JSON_DISABLED;NOT WITH_CJSON_REQUIRED;NOT WITH_JANSSON_REQUIRED" OFF
)
cmake_dependent_option(
  WITH_JANSSON_REQUIRED "Build with JANSSON (fail if not found)" OFF
  "NOT WITH_JSON_DISABLED;NOT WITH_CJSON_REQUIRED;NOT WITH_JSONC_REQUIRED" OFF
)

# ensure no package is enabled before the detection starts
unset(json-c_FOUND CACHE)
unset(cJSON_FOUND CACHE)
unset(jansson_FOUND CACHE)
if(NOT WITH_JSON_DISABLED)
  if(WITH_JANSSON_REQUIRED)
    detect_package(jansson CMAKE jansson::jansson HEADER jansson.h REQUIRED)
  elseif(WITH_JSONC_REQUIRED)
    detect_package(json-c CMAKE json-c::json-c HEADER json-c/json.h REQUIRED)
  elseif(WITH_CJSON_REQUIRED)
    detect_package(
      cJSON
      PKGCONFIG
      libcjson
      CMAKE
      cjson
      HEADER
      cjson/cJSON.h
      REQUIRED
    )
  else()
    # nothing required, so do a non fatal check for all
    detect_package(jansson CMAKE jansson::jansson HEADER jansson.h)
    detect_package(json-c CMAKE json-c::json-c HEADER json-c/json.h)
    detect_package(
      cJSON
      PKGCONFIG
      libcjson
      CMAKE
      cjson
      HEADER
      cjson/cJSON.h
    )
  endif()

  if(NOT json-c_FOUND AND NOT cJSON_FOUND AND NOT jansson_FOUND)
    if(WITH_CJSON_REQUIRED OR WITH_JSONC_REQUIRED OR WITH_JANSSON_REQUIRED)
      message(
        FATAL_ERROR
          "cJSON (${WITH_CJSON_REQUIRED}) or json-c (${WITH_JSONC_REQUIRED}) or jansson (${WITH_JANSSON_REQUIRED}) required but not found"
      )
    endif()
    set(WITH_WINPR_JSON OFF CACHE INTERNAL "internal")
    message("compiling without JSON support. Install cJSON or json-c to enable")
  endif()
else()
  set(WITH_WINPR_JSON OFF CACHE INTERNAL "internal")
  message("forced compile without JSON support. Set -DWITH_JSON_DISABLED=OFF to enable compile time detection")
endif()
