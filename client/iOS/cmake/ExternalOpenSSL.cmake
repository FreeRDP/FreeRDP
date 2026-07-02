include(ExternalProject)
include(DepVersions)

set(_ossl_arch "${CMAKE_OSX_ARCHITECTURES}")

if(PLATFORM MATCHES "SIMULATOR")
  set(_ossl_sdk "iphonesimulator")
  set(_ossl_platform "iPhoneSimulator")
  set(_ossl_config_args "iphoneos-cross" "no-asm")
  set(_ossl_cc_extra
      "-target ${_ossl_arch}-apple-ios${CMAKE_OSX_DEPLOYMENT_TARGET}-simulator -mios-simulator-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}"
  )
else()
  set(_ossl_sdk "iphoneos")
  set(_ossl_platform "iPhoneOS")
  set(_ossl_config_args "iphoneos-cross")
  set(_ossl_cc_extra "-miphoneos-version-min=${CMAKE_OSX_DEPLOYMENT_TARGET}")
endif()

execute_process(COMMAND xcode-select -print-path OUTPUT_VARIABLE _xcode_dev OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(
  COMMAND xcrun --sdk ${_ossl_sdk} --show-sdk-version OUTPUT_VARIABLE _ossl_sdk_ver OUTPUT_STRIP_TRAILING_WHITESPACE
)

set(_ossl_cross_top "${_xcode_dev}/Platforms/${_ossl_platform}.platform/Developer")
set(_ossl_cross_sdk "${_ossl_platform}${_ossl_sdk_ver}.sdk")
set(_ossl_cc "${_xcode_dev}/usr/bin/gcc -arch ${_ossl_arch} ${_ossl_cc_extra}")

set(_ossl_env "CROSS_TOP=${_ossl_cross_top}" "CROSS_SDK=${_ossl_cross_sdk}" "CC=${_ossl_cc}")

if(BUILD_SHARED_LIBS)
  set(ARGS shared)
else()
  set(ARGS no-shared)
endif()

ExternalProject_Add(
  openssl
  PREFIX ${CMAKE_BINARY_DIR}/openssl
  DOWNLOAD_EXTRACT_TIMESTAMP OFF
  # Keep sources in the build tree; ExternalProject must not write into client/iOS.
  SOURCE_DIR ${CMAKE_BINARY_DIR}/external/openssl
  BINARY_DIR ${CMAKE_BINARY_DIR}/external/openssl-build
  URL https://github.com/openssl/openssl/releases/download/${OPENSSL_VERSION}/${OPENSSL_VERSION}.tar.gz
  URL_HASH ${OPENSSL_HASH}
  CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env ${_ossl_env} perl <SOURCE_DIR>/Configure ${_ossl_config_args} ${ARGS}
                    no-tests no-apps no-docs --prefix=${DEPS_INSTALL_DIR} --libdir=${CMAKE_INSTALL_LIBDIR}
  BUILD_COMMAND ${CMAKE_COMMAND} -E env ${_ossl_env} make -j build_sw
  INSTALL_COMMAND ${CMAKE_COMMAND} -E env ${_ossl_env} make install_sw
)
