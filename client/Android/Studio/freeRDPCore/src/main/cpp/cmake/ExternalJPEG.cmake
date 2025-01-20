include(ExternalProject)

ExternalProject_Add(
  jpeg
  DOWNLOAD_COMMAND
  	GIT_REPOSITORY https://github.com/libjpeg-turbo/libjpeg-turbo.git
  	GIT_TAG        3.1.0
  CMAKE_ARGS ${CL_ARGS}
)

