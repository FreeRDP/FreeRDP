include(ExternalProject)

set(OPENH264_VERSION "v2.6.0")

# Map Android ABI to OpenH264 architecture names
if(ANDROID_ABI STREQUAL "arm64-v8a")
  set(O264_ARCH "arm64")
elseif(ANDROID_ABI STREQUAL "armeabi-v7a")
  set(O264_ARCH "arm")
elseif(ANDROID_ABI STREQUAL "x86_64")
  set(O264_ARCH "x86_64")
elseif(ANDROID_ABI STREQUAL "x86")
  set(O264_ARCH "x86")
else()
  message(FATAL_ERROR "ExternalOpenH264: unsupported ABI '${ANDROID_ABI}'")
endif()

ExternalProject_Add(
  openh264
  DOWNLOAD_EXTRACT_TIMESTAMP OFF
  SOURCE_DIR ${CMAKE_SOURCE_DIR}/external/openh264
  GIT_REPOSITORY https://github.com/cisco/openh264.git
  GIT_TAG ${OPENH264_VERSION}
  GIT_SHALLOW TRUE
  PATCH_COMMAND git am --3way ${CMAKE_CURRENT_LIST_DIR}/0001-openh264-pkgconfig-patch.patch
  CONFIGURE_COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different <SOURCE_DIR> <BINARY_DIR>
  BUILD_COMMAND
    ${CMAKE_COMMAND} -E env "PATH=${NDK_ROOT}:$ENV{PATH}" make ENABLEPIC=Yes LDFLAGS=-static-libstdc++ OS=android
    NDKROOT=${NDK_ROOT} NDK_TOOLCHAIN_VERSION=clang TARGET=android-${NDK_API_LEVEL} NDKLEVEL=${NDK_API_LEVEL}
    ARCH=${O264_ARCH} -j libraries
  INSTALL_COMMAND
    ${CMAKE_COMMAND} -E env "PATH=${NDK_ROOT}:$ENV{PATH}" make OS=android NDKROOT=${NDK_ROOT}
    NDK_TOOLCHAIN_VERSION=clang TARGET=android-${NDK_API_LEVEL} NDKLEVEL=${NDK_API_LEVEL} ARCH=${O264_ARCH}
    PREFIX=${DEPS_INSTALL_DIR} LIBDIR_NAME=${CMAKE_INSTALL_LIBDIR}
    SHAREDLIB_DIR=${DEPS_INSTALL_DIR}/${CMAKE_INSTALL_LIBDIR} -j install
)
