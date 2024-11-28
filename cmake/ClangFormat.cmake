# get all project files
file(
  GLOB_RECURSE
  ALL_SOURCE_FILES
  *.cpp
  *.c
  *.h
  *.m
  *.java
)

include(ClangDetectTool)
clang_detect_tool(CLANG_FORMAT clang-format "")

if(NOT CLANG_FORMAT)
  message(WARNING "clang-format not found in path! code format target not available.")
else()
  add_custom_target(clangformat COMMAND ${CLANG_FORMAT} -style=file -i ${ALL_SOURCE_FILES})
endif()
