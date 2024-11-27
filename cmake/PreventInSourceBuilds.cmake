# PreventInSourceBuilds
# ---------------------
#
# Prevent in-source builds
#
# It is generally acknowledged that it is preferable to run CMake out of source,
# in a dedicated build directory.  To prevent users from accidentally running
# CMake in the source directory, just include this module.

option(ALLOW_IN_SOURCE_BUILD "[deprecated] Allow building in source tree" OFF)

if(ALLOW_IN_SOURCE_BUILD)
  set(CMAKE_DISABLE_SOURCE_CHANGES OFF CACHE INTERNAL "policy")
  set(CMAKE_DISABLE_IN_SOURCE_BUILD OFF CACHE INTERNAL "policy")
  if("${srcdir}" STREQUAL "${bindir}")
    message(WARNING "Running in-source-tree build [ALLOW_IN_SOURCE_BUILD=ON]")
  endif()
else()
  set(CMAKE_DISABLE_SOURCE_CHANGES ON CACHE INTERNAL "policy")
  set(CMAKE_DISABLE_IN_SOURCE_BUILD ON CACHE INTERNAL "policy")

  # make sure the user doesn't play dirty with symlinks
  get_filename_component(srcdir "${CMAKE_SOURCE_DIR}" REALPATH)
  get_filename_component(bindir "${CMAKE_BINARY_DIR}" REALPATH)

  # disallow in-source builds
  if("${srcdir}" STREQUAL "${bindir}")
    message(
      FATAL_ERROR
        "\

CMake must not to be run in the source directory. \
Rather create a dedicated build directory and run CMake there. \
CMake now already created some files, to clean up after this aborted in-source compilation:
   rm -r CMakeCache.txt CMakeFiles
or
   git clean -xdf

If you happen to require in-source-tree builds for some reason rerun with -DALLOW_IN_SOURCE_BUILD=ON
"
    )
  endif()

  # Check for remnants of in source builds
  if(EXISTS "${CMAKE_SOURCE_DIR}/CMakeCache.txt" OR EXISTS "${CMAKE_SOURCE_DIR}/CMakeFiles")
    message(
      FATAL_ERROR
        " \

Remnants of in source CMake run detected, aborting!

To clean up after this aborted in-source compilation:
   rm -r CMakeCache.txt CMakeFiles
or
   git clean -xdf

If you happen to require in-source-tree builds for some reason rerun with -DALLOW_IN_SOURCE_BUILD=ON
"
    )
  endif()
endif()
