# - Find AAudio
# Find the AAudio includes and library (Android NDK, API 26+)
#  AAudio_FOUND - AAudio was found
#  AAudio::AAudio - imported target

find_path(AAudio_INCLUDE_DIR aaudio/AAudio.h)
find_library(AAudio_LIBRARY NAMES aaudio)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(AAudio REQUIRED_VARS AAudio_LIBRARY AAudio_INCLUDE_DIR)

if(AAudio_FOUND AND NOT TARGET AAudio::AAudio)
  add_library(AAudio::AAudio UNKNOWN IMPORTED)
  set_target_properties(
    AAudio::AAudio PROPERTIES IMPORTED_LOCATION "${AAudio_LIBRARY}" INTERFACE_INCLUDE_DIRECTORIES
                                                                    "${AAudio_INCLUDE_DIR}"
  )
endif()

mark_as_advanced(AAudio_INCLUDE_DIR AAudio_LIBRARY)
