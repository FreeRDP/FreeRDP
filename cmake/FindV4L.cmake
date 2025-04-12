find_path(V4L_INCLUDE_DIR NAMES linux/videodev2.h)

find_package_handle_standard_args(V4L DEFAULT_MSG V4L_INCLUDE_DIR)

if(V4L_FOUND)
  set(V4L_INCLUDE_DIRS ${V4L_INCLUDE_DIR})
endif()

mark_as_advanced(V4L_INCLUDE_DIR)
