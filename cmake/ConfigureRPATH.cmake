# RPATH configuration
option(CMAKE_SKIP_BUILD_RPATH "skip build RPATH" OFF)
option(CMAKE_BUILD_WITH_INSTALL_RPATH "build with install RPATH" OFF)
option(CMAKE_INSTALL_RPATH_USE_LINK_PATH "build with link RPATH" OFF)

if(APPLE)
  if(BUILD_SHARED_LIBS)
    option(CMAKE_MACOSX_RPATH "MacOSX RPATH" ON)
  endif()

  file(RELATIVE_PATH FRAMEWORK_PATH ${CMAKE_INSTALL_FULL_BINDIR} ${CMAKE_INSTALL_FULL_LIBDIR})
  set(CFG_INSTALL_RPATH "@loader_path/${FRAMEWORK_PATH}")
elseif(NOT WIN32)
  if(NOT FREEBSD)
    option(WITH_ADD_PLUGIN_TO_RPATH "Add extension and plugin path to RPATH" OFF)
    if(WITH_ADD_PLUGIN_TO_RPATH)
      set(CFG_INSTALL_RPATH
          "\$ORIGIN/../${FREERDP_EXTENSION_REL_PATH}:\$ORIGIN/../${FREERDP_PLUGIN_PATH}:\$ORIGIN/../${CMAKE_INSTALL_LIBDIR}:\$ORIGIN/.."
      )
    else()
      set(CFG_INSTALL_RPATH "\$ORIGIN/../${CMAKE_INSTALL_LIBDIR}:\$ORIGIN/..")
    endif()
  endif()
endif(APPLE)

set(CMAKE_INSTALL_RPATH ${CFG_INSTALL_RPATH} CACHE INTERNAL "ConfigureRPATH")
message("Configured RPATH=${CMAKE_INSTALL_RPATH}")

function(installWithRPATH)
  if(NOT APPLE AND NOT FREEBSD AND NOT WIN32)
    list(FIND ARGN "TARGETS" _index)
    if(${_index} GREATER -1)
      math(EXPR _index "${_index}+1")
      list(GET ARGN ${_index} target)
    else()
      message(FATAL_ERROR "Missing TARGETS for install directive")
    endif()
    list(FIND ARGN "DESTINATION" _index)
    if(${_index} GREATER -1)
      math(EXPR _index "${_index}+1")
      list(GET ARGN ${_index} path)
    else()
      message(FATAL_ERROR "Missing DESTINATION for install directive")
    endif()

    get_filename_component(ABS_PATH ${path} ABSOLUTE BASE_DIR ${CMAKE_INSTALL_PREFIX})
    file(RELATIVE_PATH REL_PATH ${ABS_PATH} ${CMAKE_INSTALL_PREFIX})
    if(WITH_ADD_PLUGIN_TO_RPATH)
      set_target_properties(
        ${target}
        PROPERTIES
          INSTALL_RPATH
          "\$ORIGIN/${REL_PATH}${FREERDP_EXTENSION_REL_PATH}:\$ORIGIN/${REL_PATH}${FREERDP_PLUGIN_PATH}:\$ORIGIN/${REL_PATH}${CMAKE_INSTALL_LIBDIR}"
      )
    else()
      set_target_properties(${target} PROPERTIES INSTALL_RPATH "\$ORIGIN/${REL_PATH}${CMAKE_INSTALL_LIBDIR}")
    endif()
  endif()

  install(${ARGN})
endfunction()
