include(CheckAndSetFlag)

option(ENABLE_WARNING_VERBOSE "enable -Weveryting (and some exceptions) for compile" OFF)
option(ENABLE_WARNING_ERROR "enable -Werror for compile" OFF)

set(COMMON_COMPILER_FLAGS "")
if(ENABLE_WARNING_VERBOSE)
  if(MSVC)
    list(APPEND COMMON_COMPILER_FLAGS /W4 /wo4324)
  else()
    list(
      APPEND
      COMMON_COMPILER_FLAGS
      -Weverything
      -Wall
      -Wpedantic
      -Wno-padded
      -Wno-switch-enum
      -Wno-cast-align
      -Wno-unsafe-buffer-usage
      -Wno-reserved-identifier
      -Wno-covered-switch-default
      -Wno-disabled-macro-expansion
      -Wno-used-but-marked-unused
      -Wno-implicit-void-ptr-cast
    )
  endif()
endif()

if(ENABLE_WARNING_ERROR)
  list(APPEND COMMON_COMPILER_FLAGS -Werror)
endif()

list(APPEND COMMON_COMPILER_FLAGS -fno-omit-frame-pointer -Wredundant-decls)
list(APPEND COMMON_COMPILER_FLAGS -fsigned-char)

include(ExportAllSymbols)
include(CompilerSanitizerOptions)

if(CMAKE_C_COMPILER_ID MATCHES ".*Clang.*" OR (CMAKE_C_COMPILER_ID MATCHES "GNU" AND CMAKE_C_COMPILER_VERSION
                                                                                     VERSION_GREATER_EQUAL 10)
)
  add_compile_options($<$<NOT:$<CONFIG:Debug>>:-fdebug-prefix-map=${CMAKE_SOURCE_DIR}=.>)
  add_compile_options($<$<NOT:$<CONFIG:Debug>>:-fmacro-prefix-map=${CMAKE_SOURCE_DIR}=.>)
  add_compile_options($<$<NOT:$<CONFIG:Debug>>:-ffile-prefix-map=${CMAKE_SOURCE_DIR}=.>)
  add_compile_options($<$<NOT:$<CONFIG:Debug>>:-fdebug-prefix-map=${CMAKE_BINARY_DIR}=./build>)
  add_compile_options($<$<NOT:$<CONFIG:Debug>>:-fmacro-prefix-map=${CMAKE_BINARY_DIR}=./build>)
  add_compile_options($<$<NOT:$<CONFIG:Debug>>:-ffile-prefix-map=${CMAKE_BINARY_DIR}=./build>)
endif()
