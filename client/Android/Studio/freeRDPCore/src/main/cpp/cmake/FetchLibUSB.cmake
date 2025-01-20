include(FetchContent)

set(LIBUSB_BUILD_EXAMPLES OFF CACHE INTERNAL "fetch content")
set(LIBUSB_BUILD_TESTING OFF CACHE INTERNAL "fetch content")
set(LIBUSB_ENABLE_DEBUG_LOGGING OFF CACHE INTERNAL "fetch content")
FetchContent_Declare(
  libusb
  GIT_REPOSITORY https://github.com/libusb/libusb-cmake.git
  GIT_TAG        v1.0.27-1
  OVERRIDE_FIND_PACKAGE
  SYSTEM
)

FetchContent_MakeAvailable(libusb)
