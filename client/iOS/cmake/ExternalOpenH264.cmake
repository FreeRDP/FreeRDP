include(ExternalProject)
include(DepVersions)

ExternalProject_Add(
  openh264 DOWNLOAD_EXTRACT_TIMESTAMP OFF GIT_REPOSITORY https://github.com/cisco/openh264.git GIT_TAG ${OPENH264_TAG}
  GIT_SHALLOW TRUE PATCH_COMMAND git am --3way ${CMAKE_CURRENT_LIST_DIR}/0002-openh264-pkgconfig-patch.patch
  CONFIGURE_COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different <SOURCE_DIR> <BINARY_DIR>
  BUILD_COMMAND make ENABLEPIC=Yes LDFLAGS=-static-libstdc++ OS=ios -j libraries
  INSTALL_COMMAND make OS=ios PREFIX=${DEPS_INSTALL_DIR} LIBDIR_NAME=${CMAKE_INSTALL_LIBDIR}
                  SHAREDLIB_DIR=${DEPS_INSTALL_DIR}/${CMAKE_INSTALL_LIBDIR} -j install
)
