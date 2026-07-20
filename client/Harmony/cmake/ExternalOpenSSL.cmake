include(ExternalProject)

set(OPENSSL_VERSION "openssl-3.6.1")
set(OPENSSL_URL_HASH "SHA256=b1bfedcd5b289ff22aee87c9d600f515767ebf45f77168cb6d64f231f518a82e")
set(OPENSSL_URL "https://github.com/openssl/openssl/releases/download/${OPENSSL_VERSION}/${OPENSSL_VERSION}.tar.gz")

set(OPENSSL_CACHE_DIR "${CMAKE_CURRENT_LIST_DIR}/../../../cache")
if(EXISTS "${OPENSSL_CACHE_DIR}/${OPENSSL_VERSION}.tar.gz")
  set(OPENSSL_URL "file://${OPENSSL_CACHE_DIR}/${OPENSSL_VERSION}.tar.gz")
endif()

if(OHOS_ARCH STREQUAL "arm64-v8a")
  set(OSSL_ARCH "ohos-aarch64")
  set(OSSL_TARGET "aarch64-linux-ohos")
elseif(OHOS_ARCH STREQUAL "armeabi-v7a")
  set(OSSL_ARCH "ohos-arm")
  set(OSSL_TARGET "armv7a-linux-ohos")
  set(OSSL_CFLAGS_EXTRA "-D__ARM_MAX_ARCH__=8")
elseif(OHOS_ARCH STREQUAL "x86_64")
  set(OSSL_ARCH "ohos-x86_64")
  set(OSSL_TARGET "x86_64-linux-ohos")
else()
  message(FATAL_ERROR "ExternalOpenSSL: unsupported OHOS_ARCH '${OHOS_ARCH}'")
endif()

get_filename_component(OHOS_TOOLCHAIN_BIN "${CMAKE_C_COMPILER}" DIRECTORY)
get_filename_component(OHOS_LLVM_DIR "${OHOS_TOOLCHAIN_BIN}" DIRECTORY)
get_filename_component(OHOS_NATIVE_DIR "${OHOS_LLVM_DIR}" DIRECTORY)
get_filename_component(OHOS_NDK_HOME "${OHOS_NATIVE_DIR}" DIRECTORY)

set(OHOS_SYSROOT "${OHOS_NDK_HOME}/native/sysroot")
set(OSSL_CFLAGS "-target ${OSSL_TARGET} --sysroot=${OHOS_SYSROOT} -D__MUSL__ ${OSSL_CFLAGS_EXTRA}")

set(HARMONY_OPENSSL_PATCH "${CMAKE_CURRENT_LIST_DIR}/../../../scripts/harmony-openssl.patch")
set(OPENSSL_INSTALL_DIR "${CMAKE_BINARY_DIR}/deps/openssl")

set(OSSL_ENV
    "PATH=${OHOS_TOOLCHAIN_BIN}:$ENV{PATH}"
    "CC=${OHOS_TOOLCHAIN_BIN}/clang"
    "CXX=${OHOS_TOOLCHAIN_BIN}/clang++"
    "AR=${OHOS_TOOLCHAIN_BIN}/llvm-ar"
    "LD=${OHOS_TOOLCHAIN_BIN}/ld.lld"
    "STRIP=${OHOS_TOOLCHAIN_BIN}/llvm-strip"
    "RANLIB=${OHOS_TOOLCHAIN_BIN}/llvm-ranlib"
    "NM=${OHOS_TOOLCHAIN_BIN}/llvm-nm"
    "CFLAGS=${OSSL_CFLAGS}"
    "CXXFLAGS=${OSSL_CFLAGS}"
)

ExternalProject_Add(
  openssl
  SOURCE_DIR ${CMAKE_BINARY_DIR}/openssl-src
  URL ${OPENSSL_URL}
  URL_HASH ${OPENSSL_URL_HASH}
  PATCH_COMMAND patch -p1 -s < ${HARMONY_OPENSSL_PATCH}
  CONFIGURE_COMMAND
    ${CMAKE_COMMAND} -E env ${OSSL_ENV} perl <SOURCE_DIR>/Configure ${OSSL_ARCH} no-shared no-module no-tests no-apps
    no-docs no-legacy no-dso no-ssl-trace --prefix=${OPENSSL_INSTALL_DIR} --libdir=${OPENSSL_INSTALL_DIR}
    --openssldir=${OPENSSL_INSTALL_DIR}
  BUILD_COMMAND ${CMAKE_COMMAND} -E env ${OSSL_ENV} make build_libs -j
  INSTALL_COMMAND ${CMAKE_COMMAND} -E make_directory ${OPENSSL_INSTALL_DIR}/include/openssl
  COMMAND ${CMAKE_COMMAND} -E copy_directory <SOURCE_DIR>/include/openssl ${OPENSSL_INSTALL_DIR}/include/openssl
  COMMAND ${CMAKE_COMMAND} -E copy_directory <BINARY_DIR>/include/openssl ${OPENSSL_INSTALL_DIR}/include/openssl
  COMMAND ${CMAKE_COMMAND} -E copy <BINARY_DIR>/libssl.a ${OPENSSL_INSTALL_DIR}/libssl.a
  COMMAND ${CMAKE_COMMAND} -E copy <BINARY_DIR>/libcrypto.a ${OPENSSL_INSTALL_DIR}/libcrypto.a
  BUILD_BYPRODUCTS ${OPENSSL_INSTALL_DIR}/libssl.a ${OPENSSL_INSTALL_DIR}/libcrypto.a
)

add_library(openssl-lib INTERFACE IMPORTED GLOBAL)
set_target_properties(
  openssl-lib PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INSTALL_DIR}/include"
                         INTERFACE_LINK_LIBRARIES "${OPENSSL_INSTALL_DIR}/libssl.a;${OPENSSL_INSTALL_DIR}/libcrypto.a"
)
add_dependencies(openssl-lib openssl)
file(MAKE_DIRECTORY ${OPENSSL_INSTALL_DIR}/include)
