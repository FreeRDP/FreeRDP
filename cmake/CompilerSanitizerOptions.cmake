include(CMakeDependentOption)
include(CheckIncludeFiles)

cmake_dependent_option(
  WITH_VALGRIND_MEMCHECK "Compile with valgrind helpers." OFF
  "NOT WITH_SANITIZE_ADDRESS; NOT WITH_SANITIZE_MEMORY; NOT WITH_SANITIZE_THREAD" OFF
)
cmake_dependent_option(
  WITH_SANITIZE_ADDRESS "Compile with gcc/clang address sanitizer." OFF
  "NOT WITH_VALGRIND_MEMCHECK; NOT WITH_SANITIZE_MEMORY; NOT WITH_SANITIZE_THREAD" OFF
)
cmake_dependent_option(
  WITH_SANITIZE_MEMORY "Compile with gcc/clang memory sanitizer." OFF
  "NOT WITH_VALGRIND_MEMCHECK; NOT WITH_SANITIZE_ADDRESS; NOT WITH_SANITIZE_THREAD" OFF
)
cmake_dependent_option(
  WITH_SANITIZE_THREAD "Compile with gcc/clang thread sanitizer." OFF
  "NOT WITH_VALGRIND_MEMCHECK; NOT WITH_SANITIZE_ADDRESS; NOT WITH_SANITIZE_MEMORY" OFF
)

if(WITH_VALGRIND_MEMCHECK)
  check_include_files(valgrind/memcheck.h FREERDP_HAVE_VALGRIND_MEMCHECK_H)
else()
  unset(FREERDP_HAVE_VALGRIND_MEMCHECK_H CACHE)
endif()

# Enable address sanitizer, where supported and when required
if(CMAKE_COMPILER_IS_CLANG OR CMAKE_COMPILER_IS_GNUCC)
  set(CMAKE_REQUIRED_LINK_OPTIONS_SAVED ${CMAKE_REQUIRED_LINK_OPTIONS})
  file(WRITE ${PROJECT_BINARY_DIR}/foo.txt "")
  if(WITH_SANITIZE_ADDRESS)
    add_compile_options(-fsanitize=address)
    add_compile_options(-fsanitize-address-use-after-scope)
    add_link_options(-fsanitize=address)
    if(CMAKE_C_COMPILER_ID MATCHES ".*Clang.*")
      add_link_options(-shared-libasan)
      foreach(CLANG_ASAN_LIBRARY_NAME "libclang_rt.asan-${CMAKE_SYSTEM_PROCESSOR}.so" "libclang_rt.asan.so")
        execute_process(
          COMMAND ${CMAKE_C_COMPILER} -print-file-name=${CLANG_ASAN_LIBRARY_NAME} OUTPUT_VARIABLE CLANG_ASAN_LIBRARY
          OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET
        )
        if(EXISTS "${CLANG_ASAN_LIBRARY}")
          get_filename_component(CLANG_ASAN_LIBRARY_DIR "${CLANG_ASAN_LIBRARY}" DIRECTORY)
          break()
        endif()
      endforeach()

      if(NOT CLANG_ASAN_LIBRARY_DIR)
        execute_process(
          COMMAND ${CMAKE_C_COMPILER} --print-resource-dir OUTPUT_VARIABLE CLANG_RESOURCE_DIR
          OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET
        )
        file(GLOB CLANG_ASAN_LIBRARIES "${CLANG_RESOURCE_DIR}/lib/*/libclang_rt.asan*.so"
             "${CLANG_RESOURCE_DIR}/lib/libclang_rt.asan*.so"
        )
        if(CLANG_ASAN_LIBRARIES)
          list(GET CLANG_ASAN_LIBRARIES 0 CLANG_ASAN_LIBRARY)
          get_filename_component(CLANG_ASAN_LIBRARY_DIR "${CLANG_ASAN_LIBRARY}" DIRECTORY)
        endif()
      endif()

      if(CLANG_ASAN_LIBRARY_DIR)
        add_link_options("-Wl,-rpath,${CLANG_ASAN_LIBRARY_DIR}")
      endif()
    endif()
  elseif(WITH_SANITIZE_MEMORY)
    add_compile_options(-fsanitize=memory)
    add_compile_options(-fsanitize-memory-use-after-dtor)
    add_compile_options(-fsanitize-memory-track-origins)
    add_link_options(-fsanitize=memory)
  elseif(WITH_SANITIZE_THREAD)
    add_compile_options(-fsanitize=thread)
    add_link_options(-fsanitize=thread)
  endif()

  option(WITH_NO_UNDEFINED "Add -Wl,--no-undefined" OFF)
  if(WITH_NO_UNDEFINED)
    add_link_options(-Wl,--no-undefined)
  endif()
endif()
