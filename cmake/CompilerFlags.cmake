include(CheckCCompilerFlag)
include(CommonCompilerFlags)

macro(checkCFlag FLAG)
  check_c_compiler_flag("${FLAG}" CFLAG${FLAG})
  if(CFLAG${FLAG})
    string(APPEND CMAKE_C_FLAGS " ${FLAG}")
  else()
    message(WARNING "compiler does not support ${FLAG}")
  endif()
endmacro()

if(ENABLE_WARNING_VERBOSE)
  if(MSVC)
    # Remove previous warning definitions,
    # NMake is otherwise complaining.
    foreach(flags_var_to_scrub CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE CMAKE_C_FLAGS_RELWITHDEBINFO
                               CMAKE_C_FLAGS_MINSIZEREL
    )
      string(REGEX REPLACE "(^| )[/-]W[ ]*[1-9]" " " "${flags_var_to_scrub}" "${${flags_var_to_scrub}}")
    endforeach()
  else()
    list(APPEND COMMON_COMPILER_FLAGS -Wno-declaration-after-statement -Wno-pre-c11-compat
         -Wno-gnu-zero-variadic-macro-arguments
    )
  endif()
endif()

list(APPEND COMMON_COMPILER_FLAGS -Wimplicit-function-declaration)

foreach(FLAG ${COMMON_COMPILER_FLAGS})
  checkcflag(${FLAG})
endforeach()

# Android profiling
if(ANDROID)
  if(WITH_GPROF)
    checkandsetflag(-pg)
    set(PROFILER_LIBRARIES "${FREERDP_EXTERNAL_PROFILER_PATH}/obj/local/${ANDROID_ABI}/libandroid-ndk-profiler.a")
    include_directories(SYSTEM "${FREERDP_EXTERNAL_PROFILER_PATH}")
  endif()
endif()

# Assume source files are utf-8 when compiling with MSVC
add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

set(CMAKE_C_FLAGS ${CMAKE_C_FLAGS} CACHE STRING "default CFLAGS")
message("Using CFLAGS ${CMAKE_C_FLAGS}")
message("Using CFLAGS_RELEASE ${CMAKE_C_FLAGS_RELEASE}")
message("Using CFLAGS_DEBUG ${CMAKE_C_FLAGS_DEBUG}")
message("Using CFLAGS_MINSIZEREL ${CMAKE_C_FLAGS_MINSIZEREL}")
message("Using CFLAGS_RELWITHDEBINFO ${CMAKE_C_FLAGS_RELWITHDEBINFO}")
