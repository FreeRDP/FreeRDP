include(FetchContent)

FetchContent_Declare(
  jpeg
  GIT_REPOSITORY https://github.com/libjpeg-turbo/libjpeg-turbo.git
  GIT_TAG        3.1.0
)

FetchContent_MakeAvailable(jpeg)
