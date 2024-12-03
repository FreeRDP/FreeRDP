if(MSVC)
  if(CMAKE_VERSION VERSION_LESS 3.15.0)
    message(FATAL_ERROR "windows builds require CMake >= 3.15")
  endif()
  if(NOT DEFINED CMAKE_MSVC_RUNTIME_LIBRARY)
    if(MSVC_RUNTIME STREQUAL "dynamic")
      set(MSVC_DEFAULT_RUNTIME "MultiThreadedDLL")
    elseif(MSVC_RUNTIME STREQUAL "static")
      set(MSVC_DEFAULT_RUNTIME "MultiThreaded")
    else()
      message(WARNING "invalid MSVC_RUNTIME (deprecated) value '${MSVC_RUNTIME}', ignoring")
    endif()

    if(MSVC_DEFAULT_RUNTIME)
      message(
        "Using CMAKE_MSVC_RUNTIME_LIBRARY=${MSVC_DEFAULT_RUNTIME} (derived from MSVC_RUNTIME (deprecated) value '${MSVC_RUNTIME}')"
      )
    else()
      set(MSVC_DEFAULT_RUNTIME "MultiThreaded")

      string(APPEND MSVC_DEFAULT_RUNTIME "$<$<CONFIG:Debug>:Debug>")

      if(BUILD_SHARED_LIBS)
        string(APPEND MSVC_DEFAULT_RUNTIME "DLL")
      endif()
      message("Using CMAKE_MSVC_RUNTIME_LIBRARY=${MSVC_DEFAULT_RUNTIME}")
    endif()

    set(CMAKE_MSVC_RUNTIME_LIBRARY ${MSVC_DEFAULT_RUNTIME} CACHE STRING "MSVC runtime")
  endif()

  message("build is using MSVC runtime ${CMAKE_MSVC_RUNTIME_LIBRARY}")

  string(FIND ${CMAKE_MSVC_RUNTIME_LIBRARY} "DLL" IS_SHARED)
  if(IS_SHARED STREQUAL "-1")
    if(BUILD_SHARED_LIBS)
      message(FATAL_ERROR "Static CRT is only supported in a fully static build")
    endif()
    message(STATUS "Use the MSVC static runtime option carefully!")
    message(STATUS "OpenSSL uses /MD by default, and is very picky")
    message(STATUS "Random freeing errors are a common sign of runtime issues")
  endif()

  if(NOT DEFINED CMAKE_SUPPRESS_REGENERATION)
    set(CMAKE_SUPPRESS_REGENERATION ON)
  endif()
endif()
