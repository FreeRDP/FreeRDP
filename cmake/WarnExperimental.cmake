set(WARN_EXPERIMENTAL_LIST_PRIVATE "" CACHE INTERNAL "dependencies")

function(warn_experimental name)
  set(CUR_NAMES ${name})
  if(ARGN)
    string(APPEND CUR_NAMES ", deactivate with")
    foreach(_lib ${ARGN})
      string(APPEND CUR_NAMES " ${_lib}")
    endforeach()
  endif()
  list(APPEND WARN_EXPERIMENTAL_LIST_PRIVATE ${CUR_NAMES})
  set(WARN_EXPERIMENTAL_LIST_PRIVATE "${WARN_EXPERIMENTAL_LIST_PRIVATE}" CACHE INTERNAL "dependencies")
endfunction()

function(warn_experimental_print)
  if(WARN_EXPERIMENTAL_LIST_PRIVATE)
    foreach(_lib ${WARN_EXPERIMENTAL_LIST_PRIVATE})
      message(WARNING "[experimental] ${_lib}")
    endforeach()

    string(APPEND MESSAGE "[experimental] No API/ABI guarantees of the stable version apply.")
    string(APPEND MESSAGE "\n[experimental] The feature can change with every version, or even be dropped entirely.")
    string(
      APPEND
      MESSAGE
      "\n[experimental] If problems occur please check https://github.com/FreeRDP/FreeRDP/issues for known issues, but be prepared to fix them on your own!"
    )
    string(APPEND MESSAGE
           "\n[experimental] Developers hang out in https://matrix.to/#/#FreeRDP:matrix.org?via=matrix.org"
    )
    string(
      APPEND
      MESSAGE
      "\n[experimental] - don't hesitate to ask some questions. (replies might take some time depending on your timezone)"
    )
    string(APPEND MESSAGE "\n[experimental] - if you intend using this component write us a message")
    string(APPEND MESSAGE "\n[experimental] use ${ARGN} to disable")
    message(WARNING "${MESSAGE}")
  endif()
endfunction()
