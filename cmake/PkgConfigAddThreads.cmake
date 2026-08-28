if(CMAKE_VERSION GREATER_EQUAL "3.18")
  include(CheckLinkerFlag)
endif()

function(pkg_config_add_threads var)
  set(LIBS "")
  find_package(Threads REQUIRED)

  foreach(LIB IN LISTS ARGN)
    if(CMAKE_VERSION GREATER_EQUAL "3.18")
      check_linker_flag(C "-l${LIB}" HAVE_LIB_${LIB})
    else()
      message(
        WARNING
          "check_linker_flag not available with CMake < 3.18. Unconditionally adding -l${LIB} to pkg-config link library list"
      )
      set(HAVE_LIB_${LIB} ON)
    endif()

    if(HAVE_LIB_${LIB})
      list(APPEND LIBS "-l${LIB}")
    endif()
  endforeach()

  list(APPEND LIBS ${CMAKE_THREAD_LIBS_INIT})
  list(PREPEND LIBS ${${var}})
  string(REPLACE ";" " " LIBS "${LIBS}")
  set(${var} ${LIBS} PARENT_SCOPE)
endfunction()
