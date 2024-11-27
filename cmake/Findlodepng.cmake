# - Try to find lodepng
# Once done this will define
#  lodepng_FOUND - lodepng was found
#  lodepng_INCLUDE_DIRS - lodepng include directories
#  lodepng_LIBRARIES - lodepng libraries for linking

find_path(lodepng_INCLUDE_DIR NAMES lodepng.h)

find_library(lodepng_LIBRARY NAMES lodepng)

if(lodepng_INCLUDE_DIR AND lodepng_LIBRARY)
  set(lodepng_FOUND ON)
  set(lodepng_INCLUDE_DIRS ${lodepng_INCLUDE_DIR})
  set(lodepng_LIBRARIES ${lodepng_LIBRARY})
endif()

mark_as_advanced(lodepng_INCLUDE_DIRS lodepng_LIBRARIES)
