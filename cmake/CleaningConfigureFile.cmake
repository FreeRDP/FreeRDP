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
  get_filename_component(DST_NAME ${DST} NAME)

  # ensure the temporary configuration files are in a different directory
  set(CFG_DIR "${CMAKE_CURRENT_BINARY_DIR}/cfg")
  file(MAKE_DIRECTORY "${CFG_DIR}")

  # Generate the temporary configuration files
  if(CMAKE_CONFIGURATION_TYPES)
    foreach(CFG ${CMAKE_CONFIGURATION_TYPES})
      set(CURRENT_BUILD_CONFIG ${CFG})
      if(NOT SOURCE_CFG_INIT)
        set(SOURCE_CFG_INIT "${CFG_DIR}/${DST_NAME}_${CFG}")
      endif()
      cflagstovar(CURRENT_C_FLAGS ${CURRENT_BUILD_CONFIG})
      configure_file(${SRC} "${CFG_DIR}/${DST_NAME}_${CFG}" ${ARGN})
      unset(CURRENT_BUILD_CONFIG)
      unset(CURRENT_C_FLAGS)
    endforeach()
    set(SOURCE_CFG "${CFG_DIR}/${DST_NAME}_$<CONFIG>")
  else()
    # We call this also from CMake scripts without a CMAKE_BUILD_TYPE
    # Fall back to an explicitly unsupported build type to point out something is wrong
    # if this variable is used during such a call
    if(CMAKE_BUILD_TYPE)
      set(CURRENT_BUILD_CONFIG ${CMAKE_BUILD_TYPE})
    else()
      set(CURRENT_BUILD_CONFIG "InvalidBuildType")
    endif()
    cflagstovar(CURRENT_C_FLAGS ${CURRENT_BUILD_CONFIG})
    set(SOURCE_CFG "${CFG_DIR}/${DST_NAME}_${CMAKE_BUILD_TYPE}")
    set(SOURCE_CFG_INIT "${SOURCE_CFG}")
    configure_file(${SRC} "${SOURCE_CFG}" ${ARGN})
    unset(CURRENT_BUILD_CONFIG)
    unset(CURRENT_C_FLAGS)
  endif()

  # Fallback for older CMake: we want to copy only if the destination is different.
  # First do an initial copy during configuration
  file(MAKE_DIRECTORY "${DST_DIR}")
  if(CMAKE_VERSION VERSION_LESS "3.21.0")
    get_filename_component(CFG_VAR ${SOURCE_CFG_INIT} NAME)
    file(COPY ${SOURCE_CFG_INIT} DESTINATION ${DST_DIR})
    set(CFG_VAR_ABS "${DST_DIR}/${CFG_VAR}")
    file(RENAME ${CFG_VAR_ABS} ${DST})
  else()
    file(COPY_FILE ${SOURCE_CFG_INIT} ${DST} ONLY_IF_DIFFERENT)
  endif()

  # Create a target to recreate the configuration file if something changes.
  string(SHA256 DST_HASH "${DST}")
  string(SUBSTRING "${DST_HASH}" 0 8 DST_HASH)
  if(NOT TARGET ct-${DST_HASH})
    add_custom_target(
      ct-${DST_HASH} COMMAND ${CMAKE_COMMAND} "-E" "make_directory" "${DST_DIR}"
      COMMAND ${CMAKE_COMMAND} "-E" "copy_if_different" "${SOURCE_CFG}" "${DST}" DEPENDS ${SOURCE_CFG} ${DST}
    )
  endif()
endfunction()
