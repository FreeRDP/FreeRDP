set(WARN_UNMAINTAINED_LIST_PRIVATE "" CACHE INTERNAL "dependencies")

function(warn_unmaintained name)
  set(CUR_NAMES ${name})
  list(LENGTH ARGN ARGN_LEN)
  if(ARGN)
    string(APPEND CUR_NAMES ", deactivate with")
    foreach(_lib ${ARGN})
      string(APPEND CUR_NAMES " ${_lib}")
    endforeach()
  endif()
  list(APPEND WARN_UNMAINTAINED_LIST_PRIVATE ${CUR_NAMES})
endfunction()

function(warn_unmaintained_print)
  if(WARN_UNMAINTAINED_LIST_PRIVATE)
    foreach(_lib ${WARN_UNMAINTAINED_LIST_PRIVATE})
      message(WARNING "[unmaintained] ${_lib}")
    endforeach()
    string(APPEND MESSAGE "[unmaintained] use at your own risk!")
    string(
      APPEND
      MESSAGE
      "\n[unmaintained] If problems occur please check https://github.com/FreeRDP/FreeRDP/issues for known issues, but be prepared to fix them on your own!"
    )
    string(APPEND MESSAGE
           "\n[unmaintained] Developers hang out in https://matrix.to/#/#FreeRDP:matrix.org?via=matrix.org"
    )
    string(
      APPEND
      MESSAGE
      "\n[unmaintained] - don't hesitate to ask some questions. (replies might take some time depending on your timezone)"
    )
    string(APPEND MESSAGE "\n[unmaintained] - if you intend using this component write us a message")
    string(APPEND MESSAGE "\n[unmaintained] use ${ARGN} to disable")
    message(WARNING "${MESSAGE}")
  endif()
endfunction()
