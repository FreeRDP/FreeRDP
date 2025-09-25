function(detect_package name pkgconf cmake-target)
  set(options REQUIRED)
  cmake_parse_arguments(PARSE_ARGV 0 arg "${options}" "${oneValueArgs}" "${multiValueArgs}")

  find_package(${name})
  if(NOT ${name}_FOUND)
    # Fallback detection:
    # older ubuntu releases did not ship CMake or pkg-config files
    # for ${pkgconf}. Be optimistic and try pkg-config and as last resort
    # try manual detection
    find_package(PkgConfig)
    if(PKG_CONFIG_FOUND)
      pkg_check_modules(${name} ${pkgconf})

      if(${name}_FOUND)
        # Create imported target cjson
        add_library(${cmake-target} SHARED IMPORTED)
        set_property(TARGET ${cmake-target} APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
        set_target_properties(
          ${cmake-target}
          PROPERTIES IMPORTED_CONFIGURATIONS NOCONFIG IMPORTED_LINK_INTERFACE_LANGUAGES_NOCONFIG "C"
                     IMPORTED_LOCATION_NOCONFIG "${${name}_LINK_LIBRARIES}" IMPORTED_SONAME_NOCONFIG
                                                                            "${${name}_LIBRARIES}"
                     INTERFACE_INCLUDE_DIRECTORIES "${${name}_INCLUDE_DIRS}"
        )
      endif()
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
    detect_package(jansson jansson jansson::jansson REQUIRED)
  elseif(WITH_JSONC_REQUIRED)
    detect_package(json-c json-c json-c::json-c REQUIRED)
  elseif(WITH_CJSON_REQUIRED)
    detect_package(cJSON libcjson cjson REQUIRED)
  else()
    # nothing required, so do a non fatal check for all
    detect_package(jansson jansson jansson::jansson)
    detect_package(json-c json-c json-c::json-c)
    detect_package(cJSON libcjson cjson)
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
