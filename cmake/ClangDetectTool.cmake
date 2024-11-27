function(clang_detect_tool VAR NAME OPTS)
  set(NAMES "")
  foreach(CNT RANGE 12 22)
    list(APPEND NAMES "${NAME}-${CNT}")
  endforeach()
  list(REVERSE NAMES)
  list(APPEND NAMES ${NAME})

  find_program(${VAR} NAMES ${NAMES} ${OPTS})
  if(NOT ${VAR})
    message(WARNING "clang tool ${NAME} (${VAR}) not detected, skipping")
    unset(${VAR})
    return()
  endif()

  execute_process(
    COMMAND ${${VAR}} "--version" OUTPUT_VARIABLE _CLANG_TOOL_VERSION RESULT_VARIABLE _CLANG_TOOL_VERSION_FAILED
  )

  if(_CLANG_TOOL_VERSION_FAILED)
    message(WARNING "A problem was encountered with ${${VAR}}")
    message(WARNING "${_CLANG_TOOL_VERSION_FAILED}")
    unset(${VAR})
    return()
  endif()

  string(REGEX MATCH "([7-9]|[1-9][0-9])\\.[0-9]\\.[0-9]" CLANG_TOOL_VERSION "${_CLANG_TOOL_VERSION}")

  if(NOT CLANG_TOOL_VERSION)
    message(WARNING "problem parsing ${NAME} version for ${${VAR}}")
    unset(${VAR})
    return()
  endif()

  set(_CLANG_TOOL_MINIMUM_VERSION "12.0.0")
  if(${CLANG_TOOL_VERSION} VERSION_LESS ${_CLANG_TOOL_MINIMUM_VERSION})
    message(WARNING "clang-format version ${CLANG_TOOL_VERSION} not supported")
    message(WARNING "Minimum version required: ${_CLANG_TOOL_MINIMUM_VERSION}")
    unset(${VAR})
    return()
  endif()
endfunction()
