# get all project files
# Only the folders holding project sources are scanned: globbing the whole source root would
# also walk (and reformat) build directories, fetched dependencies and virtualenvs created in
# the tree, which can make every configure take a long time.
set(CLANG_FORMAT_SOURCE_DIRS
    channels
    client
    include
    libfreerdp
    rdtk
    resources
    scripts
    server
    uwac
    winpr
)

set(CLANG_FORMAT_GLOBS "")
foreach(dir ${CLANG_FORMAT_SOURCE_DIRS})
  foreach(ext cpp c h m java)
    list(APPEND CLANG_FORMAT_GLOBS "${CMAKE_CURRENT_SOURCE_DIR}/${dir}/*.${ext}")
  endforeach()
endforeach()

file(GLOB_RECURSE ALL_SOURCE_FILES ${CLANG_FORMAT_GLOBS})

include(ClangDetectTool)
clang_detect_tool(CLANG_FORMAT clang-format "")

if(NOT CLANG_FORMAT)
  message(WARNING "clang-format not found in path! code format target not available.")
else()
  add_custom_target(clangformat COMMAND ${CLANG_FORMAT} -style=file -i ${ALL_SOURCE_FILES})
endif()
