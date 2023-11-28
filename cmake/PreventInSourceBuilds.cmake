# PreventInSourceBuilds
# ---------------------
#
# Prevent in-source builds
#
# It is generally acknowledged that it is preferable to run CMake out of source,
# in a dedicated build directory.  To prevent users from accidentally running
# CMake in the source directory, just include this module.

# make sure the user doesn't play dirty with symlinks
get_filename_component(srcdir "${CMAKE_SOURCE_DIR}" REALPATH)
get_filename_component(bindir "${CMAKE_BINARY_DIR}" REALPATH)

# disallow in-source builds
if("${srcdir}" STREQUAL "${bindir}")
     message(FATAL_ERROR "\

CMake must not to be run in the source directory. \
Rather create a dedicated build directory and run CMake there. \
CMake now already created some files, to clean up after this aborted in-source compilation:
   rm -r CMakeCache.txt CMakeFiles
or
   git clean -xdf
")
endif()

# Check for remnants of in source builds
if(EXISTS "${CMAKE_SOURCE_DIR}/CMakeCache.txt" OR EXISTS "${CMAKE_SOURCE_DIR}/CMakeFiles")
	message(FATAL_ERROR " \

Remnants of in source CMake run detected, aborting!

To clean up after this aborted in-source compilation:
   rm -r CMakeCache.txt CMakeFiles
or
   git clean -xdf
")
endif()
