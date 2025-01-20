include(FetchContent)

set(PNG_TESTS OFF CACHE INTERNAL "fetch content")
set(PNG_TOOLS OFF CACHE INTERNAL "fetch content")
FetchContent_Declare(
  png
  OVERRIDE_FIND_PACKAGE
  SYSTEM
  GIT_REPOSITORY https://github.com/pnggroup/libpng.git
  GIT_TAG        v1.6.45
)

FetchContent_MakeAvailable(png)
