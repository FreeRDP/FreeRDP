# Little helper that adds the generated file to the
# files to be cleaned in the current directory.
#
# Handy if the generated files might have changed
#

include(CFlagsToVar)

function(cleaning_configure_file RSRC RDST)
  get_filename_component(SRC "${RSRC}" ABSOLUTE BASE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
  get_filename_component(DST "${RDST}" ABSOLUTE BASE_DIR ${CMAKE_CURRENT_BINARY_DIR})
  get_filename_component(DST_DIR "${DST}" DIRECTORY)
  if(CMAKE_CONFIGURATION_TYPES)
    foreach(CFG ${CMAKE_CONFIGURATION_TYPES})
      set(CURRENT_BUILD_CONFIG ${CFG})
      cflagstovar(CURRENT_C_FLAGS ${CURRENT_BUILD_CONFIG})
      configure_file(${SRC} ${DST}_${CFG} ${ARGN})
      unset(CURRENT_BUILD_CONFIG)
      unset(CURRENT_C_FLAGS)
    endforeach()
  else()
    # We call this also from CMake scripts without a CMAKE_BUILD_TYPE
    # Fall back to an explicitly unsupported build type to point out something is wrond
    # if this variable is used during such a call
    if(CMAKE_BUILD_TYPE)
      set(CURRENT_BUILD_CONFIG ${CMAKE_BUILD_TYPE})
    else()
      set(CURRENT_BUILD_CONFIG "InvalidBuildType")
    endif()
    cflagstovar(CURRENT_C_FLAGS ${CURRENT_BUILD_CONFIG})
    configure_file(${SRC} ${DST}_${CMAKE_BUILD_TYPE} ${ARGN})
    unset(CURRENT_BUILD_CONFIG)
    unset(CURRENT_C_FLAGS)
  endif()

  configure_file(${SRC} ${DST} ${ARGN})

  string(SHA256 DST_HASH "${DST}")
  if(NOT TARGET ct-${DST_HASH})
    add_custom_target(
      ct-${DST_HASH} ALL COMMAND ${CMAKE_COMMAND} "-E" "make_directory" "${DST_DIR}"
      COMMAND ${CMAKE_COMMAND} "-E" "copy" "${DST}_$<CONFIG>" "${DST}" COMMAND ${CMAKE_COMMAND} "-E" "sha256sum"
                                                                               "${DST}_$<CONFIG>" > ${DST}.hash
      DEPENDS ${DST} BYPRODUCTS ${DST}.hash
    )
  endif()
endfunction()
