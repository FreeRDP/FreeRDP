include(CheckLinkerFlag)

function(pkg_config_add_threads var)
  set(LIBS "")
  find_package(Threads REQUIRED)

  foreach(LIB IN LISTS ARGN)
    check_linker_flag(C "-l${LIB}" HAVE_LIB_${LIB})
    if(HAVE_LIB_${LIB})
      list(APPEND LIBS "-l${LIB}")
    endif()
  endforeach()

  list(APPEND LIBS ${CMAKE_THREAD_LIBS_INIT})
  list(PREPEND LIBS ${${var}})
  string(REPLACE ";" " " LIBS "${LIBS}")
  set(${var} ${LIBS} PARENT_SCOPE)
endfunction()
