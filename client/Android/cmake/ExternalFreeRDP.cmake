include(ExternalProject)

find_package(PkgConfig REQUIRED)

set(FREERDP_PREFIX_PATH "${DEPS_INSTALL_DIR}")
set(FREERDP_LINKER_FLAGS "-L${DEPS_INSTALL_DIR}/lib")

set(FREERDP_EXTRA_CMAKE_ARGS
    -DFREERDP_EXTERNAL_PATH:PATH=${DEPS_EXTERNAL_ROOT}
    -DWITHOUT_WINPR_3x_DEPRECATED:BOOL=ON
    -DWITHOUT_FREERDP_3x_DEPRECATED:BOOL=ON
    -DWITH_CLIENT_SDL:BOOL=OFF
    -DWITH_SAMPLE:BOOL=OFF
    -DWITH_SERVER:BOOL=OFF
    -DWITH_MANPAGES:BOOL=OFF
    -DWITH_SYSTEMD:BOOL=OFF
    # Toolchain
    -DPKG_CONFIG_EXECUTABLE:FILEPATH=${PKG_CONFIG_EXECUTABLE}
    # Internal crypto
    -DWITH_INTERNAL_RC4:BOOL=ON
    -DWITH_INTERNAL_MD4:BOOL=ON
    -DWITH_INTERNAL_MD5:BOOL=ON
    # SSL backend
    -DWITH_OPENSSL:BOOL=${WITH_OPENSSL}
    # Codecs
    -DWITH_MEDIACODEC:BOOL=${WITH_MEDIACODEC}
    -DWITH_FFMPEG:BOOL=${WITH_FFMPEG}
    -DWITH_SWSCALE:BOOL=${WITH_FFMPEG}
    -DWITH_OPENH264:BOOL=${WITH_OPENH264}
    -DWITH_VAAPI_H264_ENCODING:BOOL=OFF
    # Image formats
    -DWINPR_UTILS_IMAGE_PNG:BOOL=${WITH_PNG}
    -DWINPR_UTILS_IMAGE_WEBP:BOOL=${WITH_WEBP}
    -DWINPR_UTILS_IMAGE_JPEG:BOOL=${WITH_JPEG}
    # Audio
    -DWITH_OPUS:BOOL=${WITH_OPUS}
    # JSON
    -DWITH_CJSON_REQUIRED:BOOL=${WITH_CJSON}
    # Printer channel
    -DCHANNEL_PRINTER_CLIENT:BOOL=ON
)

# libwebp 1.3+ splits SharpYuv into its own library; pkg-config does not pull it in.
if(WITH_WEBP)
  string(APPEND FREERDP_LINKER_FLAGS " -lsharpyuv")
endif()

ExternalProject_Add(
  freerdp
  SOURCE_DIR ${FREERDP_SOURCE_DIR}
  CMAKE_COMMAND ${CMAKE_COMMAND}
  -E env "PKG_CONFIG_LIBDIR=${DEPS_INSTALL_DIR}/lib/pkgconfig" ${CMAKE_COMMAND}
  CMAKE_ARGS ${ANDROID_CMAKE_ARGS} -DCMAKE_INSTALL_PREFIX:PATH=${DEPS_INSTALL_DIR} -DCMAKE_INSTALL_LIBDIR:STRING=lib
             -DCMAKE_PREFIX_PATH:PATH=${FREERDP_PREFIX_PATH} -DCMAKE_SHARED_LINKER_FLAGS:STRING=${FREERDP_LINKER_FLAGS}
             ${FREERDP_EXTRA_CMAKE_ARGS}
  DEPENDS ${ANDROID_NATIVE_DEPS}
  BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR>
  INSTALL_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --target install
  # Tell Ninja these files are produced by this ExternalProject so it
  # doesn't complain about missing inputs when linking freerdp-android.so
  BUILD_BYPRODUCTS ${DEPS_INSTALL_DIR}/lib/libfreerdp3.so ${DEPS_INSTALL_DIR}/lib/libfreerdp-client3.so
                   ${DEPS_INSTALL_DIR}/lib/libwinpr3.so
)

file(MAKE_DIRECTORY ${DEPS_INSTALL_DIR}/include ${DEPS_INSTALL_DIR}/include/freerdp3 ${DEPS_INSTALL_DIR}/include/winpr3)

add_library(freerdp3-lib SHARED IMPORTED GLOBAL)
set_target_properties(
  freerdp3-lib PROPERTIES IMPORTED_LOCATION "${DEPS_INSTALL_DIR}/lib/libfreerdp3.so"
                          INTERFACE_INCLUDE_DIRECTORIES "${DEPS_INSTALL_DIR}/include/freerdp3"
)
add_dependencies(freerdp3-lib freerdp)

add_library(freerdp-client3-lib SHARED IMPORTED GLOBAL)
set_target_properties(
  freerdp-client3-lib PROPERTIES IMPORTED_LOCATION "${DEPS_INSTALL_DIR}/lib/libfreerdp-client3.so"
                                 INTERFACE_INCLUDE_DIRECTORIES "${DEPS_INSTALL_DIR}/include/freerdp3"
)
add_dependencies(freerdp-client3-lib freerdp)

add_library(winpr3-lib SHARED IMPORTED GLOBAL)
set_target_properties(
  winpr3-lib PROPERTIES IMPORTED_LOCATION "${DEPS_INSTALL_DIR}/lib/libwinpr3.so" INTERFACE_INCLUDE_DIRECTORIES
                                                                                 "${DEPS_INSTALL_DIR}/include/winpr3"
)
add_dependencies(winpr3-lib freerdp)

# Copy built .so files into jniLibs/ so Gradle can package them
add_custom_command(
  TARGET freerdp
  POST_BUILD
  COMMAND
    ${CMAKE_COMMAND} -DSRC_DIR=${DEPS_INSTALL_DIR}/lib
    -DDST_DIR=${CMAKE_CURRENT_SOURCE_DIR}/../jniLibs/${CMAKE_ANDROID_ARCH_ABI} -P
    ${FREERDP_SOURCE_DIR}/client/Android/cmake/CopySharedLibs.cmake
  COMMENT "Copying .so files to jniLibs/${CMAKE_ANDROID_ARCH_ABI}"
)
