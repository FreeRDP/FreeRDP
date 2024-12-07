# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP cmake build script
#
# Copyright 2011 O.S. Systems Software Ltda.
# Copyright 2011 Otavio Salvador <otavio@ossystems.com.br>
# Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
# Copyright 2012 HP Development Company, LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.13)

if(POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW)
endif()
project(FreeRDP LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

add_custom_target(fuzzers COMMENT "Build fuzzers")

if(NOT DEFINED VENDOR)
  set(VENDOR "FreeRDP" CACHE STRING "FreeRDP package vendor")
endif()

if(NOT DEFINED PRODUCT)
  set(PRODUCT "FreeRDP" CACHE STRING "FreeRDP package name")
endif()

if(NOT DEFINED FREERDP_VENDOR)
  set(FREERDP_VENDOR 1)
endif()

if(NOT WIN32 AND NOT ANDROID)
  if(APPLE)
    set(OPT_DEFAULT_VAL OFF)
  else()
    set(OPT_DEFAULT_VAL ON)
  endif()
  option(WITH_X11 "build X11 client/server" ${OPT_DEFAULT_VAL})
endif()

# Enable coverity related pragma definitions
if(COVERITY_BUILD)
  add_compile_definitions(COVERITY_BUILD)
endif()

# Include our extra modules
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/)

# Check for cmake compatibility (enable/disable features)
include(CheckCmakeCompat)

# Include cmake modules
if(WITH_CLANG_FORMAT)
  include(ClangFormat)
endif()

include(CompilerFlags)
include(CheckIncludeFiles)
include(CheckLibraryExists)
include(CheckSymbolExists)
include(CheckStructHasMember)
include(TestBigEndian)
include(CompilerDetect)

include(FindFeature)
include(ShowCMakeVars)
include(ConfigOptions)
include(FeatureSummary)
include(CheckCCompilerFlag)
include(CMakePackageConfigHelpers)
include(InstallFreeRDPMan)
include(GetGitRevisionDescription)
include(SetFreeRDPCMakeInstallDir)
include(Doxygen)

# Soname versioning
set(BUILD_NUMBER 0)
if($ENV{BUILD_NUMBER})
  set(BUILD_NUMBER $ENV{BUILD_NUMBER})
endif()

set(VERSION_REGEX "^(.*)([0-9]+)\\.([0-9]+)\\.([0-9]+)-?(.*)")
set(RAW_VERSION_STRING "3.9.1-dev0")
if(EXISTS "${PROJECT_SOURCE_DIR}/.source_tag")
  file(READ ${PROJECT_SOURCE_DIR}/.source_tag RAW_VERSION_STRING)
elseif(USE_VERSION_FROM_GIT_TAG)
  git_get_exact_tag(_GIT_TAG --tags --always)
  if(NOT ${_GIT_TAG} STREQUAL "n/a")
    string(REGEX MATCH ${VERSION_REGEX} FOUND_TAG "${_GIT_TAG}")
    if(FOUND_TAG)
      set(RAW_VERSION_STRING ${_GIT_TAG})
    endif()
  endif()
endif()
string(STRIP ${RAW_VERSION_STRING} RAW_VERSION_STRING)

string(REGEX REPLACE "${VERSION_REGEX}" "\\2" FREERDP_VERSION_MAJOR "${RAW_VERSION_STRING}")
string(REGEX REPLACE "${VERSION_REGEX}" "\\3" FREERDP_VERSION_MINOR "${RAW_VERSION_STRING}")
string(REGEX REPLACE "${VERSION_REGEX}" "\\4" FREERDP_VERSION_REVISION "${RAW_VERSION_STRING}")
string(REGEX REPLACE "${VERSION_REGEX}" "\\5" FREERDP_VERSION_SUFFIX "${RAW_VERSION_STRING}")

set(FREERDP_API_VERSION "${FREERDP_VERSION_MAJOR}")
set(FREERDP_VERSION "${FREERDP_VERSION_MAJOR}.${FREERDP_VERSION_MINOR}.${FREERDP_VERSION_REVISION}")
if(FREERDP_VERSION_SUFFIX)
  set(FREERDP_VERSION_FULL "${FREERDP_VERSION}-${FREERDP_VERSION_SUFFIX}")
else()
  set(FREERDP_VERSION_FULL "${FREERDP_VERSION}")
endif()
message("FREERDP_VERSION=${FREERDP_VERSION_FULL}")

if(EXISTS "${PROJECT_SOURCE_DIR}/.source_version")
  file(READ ${PROJECT_SOURCE_DIR}/.source_version GIT_REVISION)

  string(STRIP ${GIT_REVISION} GIT_REVISION)
elseif(USE_VERSION_FROM_GIT_TAG)
  git_get_exact_tag(GIT_REVISION --tags --always)

  if(${GIT_REVISION} STREQUAL "n/a")
    git_rev_parse(GIT_REVISION --short)
  endif()
endif()

if(NOT GIT_REVISION)
  set(GIT_REVISION ${FREERDP_VERSION})
endif()

message(STATUS "Git Revision ${GIT_REVISION}")

# MSVC compatibility with system headers
add_compile_definitions(NONAMELESSUNION)

# Make the detected version available as default version for all subprojects
set(FREERDP_DEFAULT_PROJECT_VERSION ${FREERDP_VERSION} CACHE STRING INTERNAL)

set(FREERDP_MAJOR_DIR "freerdp${FREERDP_VERSION_MAJOR}")
set(FREERDP_INCLUDE_DIR "include/${FREERDP_MAJOR_DIR}/")

option(WITH_SMARTCARD_EMULATE "Emulate smartcards instead of redirecting readers" ON)
if(WITH_SMARTCARD_EMULATE)
  add_compile_definitions(WITH_SMARTCARD_EMULATE)
  find_package(ZLIB REQUIRED)
endif()

option(WITH_FREERDP_DEPRECATED "Build FreeRDP deprecated symbols" OFF)
if(WITH_FREERDP_DEPRECATED)
  add_compile_definitions(WITH_FREERDP_DEPRECATED)
endif()

option(WITH_FREERDP_DEPRECATED_COMMANDLINE "Build FreeRDP deprecated command line options" OFF)
if(WITH_FREERDP_DEPRECATED_COMMANDLINE)
  add_compile_definitions(WITH_FREERDP_DEPRECATED_COMMANDLINE)
endif()

# Make paths absolute
if(CMAKE_INSTALL_PREFIX)
  get_filename_component(CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}" ABSOLUTE)
endif()
if(FREERDP_EXTERNAL_PATH)
  get_filename_component(FREERDP_EXTERNAL_PATH "${FREERDP_EXTERNAL_PATH}" ABSOLUTE)
endif()

# Allow to search the host machine for git/ccache
if(CMAKE_CROSSCOMPILING)
  set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM BOTH)
endif(CMAKE_CROSSCOMPILING)

find_program(CCACHE ccache)
if(CCACHE AND WITH_CCACHE)
  if(NOT DEFINED CMAKE_C_COMPILER_LAUNCHER)
    set(CMAKE_C_COMPILER_LAUNCHER ${CCACHE})
  endif(NOT DEFINED CMAKE_C_COMPILER_LAUNCHER)
endif(CCACHE AND WITH_CCACHE)

if(CMAKE_CROSSCOMPILING)
  set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM ONLY)
endif(CMAKE_CROSSCOMPILING)
# /Allow to search the host machine for git/ccache

# Turn on solution folders (2.8.4+)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(CTEST_OUTPUT_ON_FAILURE ON "show verbose output on CTest failures")
if(BUILD_TESTING_INTERNAL)
  set(EXPORT_ALL_SYMBOLS ON CACHE BOOL "testing default" FORCE)
  add_compile_definitions(BUILD_TESTING_INTERNAL)
elseif(BUILD_TESTING)
  set(EXPORT_ALL_SYMBOLS OFF CACHE BOOL "testing default" FORCE)
else()
  option(EXPORT_ALL_SYMBOLS "Export all symbols form library" OFF)
endif()

if(EXPORT_ALL_SYMBOLS)
  #	set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
  add_compile_definitions(EXPORT_ALL_SYMBOLS)
endif(EXPORT_ALL_SYMBOLS)

# Compiler-specific flags
if(CMAKE_COMPILER_IS_GNUCC)
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64" OR CMAKE_SYSTEM_PROCESSOR MATCHES "i686")
    check_symbol_exists(__x86_64__ "" IS_X86_64)
    if(IS_X86_64)
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fPIC")
    else()
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=i686")
    endif()
  endif()

  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")

  if(NOT EXPORT_ALL_SYMBOLS)
    message(STATUS "GCC default symbol visibility: hidden")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")
  endif()
  check_c_compiler_flag(-Wimplicit-function-declaration Wimplicit-function-declaration)
  if(Wimplicit-function-declaration)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wimplicit-function-declaration")
  endif()

  if(NOT OPENBSD)
    check_c_compiler_flag(-Wredundant-decls Wredundant-decls)
    if(Wredundant-decls)
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wredundant-decls")
    endif()
  endif()
  add_compile_definitions("$<$<CONFIG:Release>:NDEBUG>")
  add_compile_definitions("$<$<CONFIG:MinSizeRel>:NDEBUG>")
  add_compile_options("$<$<CONFIG:RelWithDebInfo>:-g>")
  add_compile_options("$<$<CONFIG:Debug>:-g>")
endif()

set(THREAD_PREFER_PTHREAD_FLAG TRUE)

if(NOT IOS)
  find_package(Threads REQUIRED)
endif()

# Enable address sanitizer, where supported and when required
if(CMAKE_COMPILER_IS_CLANG OR CMAKE_COMPILER_IS_GNUCC)
  set(CMAKE_REQUIRED_LINK_OPTIONS_SAVED ${CMAKE_REQUIRED_LINK_OPTIONS})
  file(WRITE ${PROJECT_BINARY_DIR}/foo.txt "")
  if(WITH_SANITIZE_ADDRESS)
    list(APPEND CMAKE_REQUIRED_LINK_OPTIONS "-fsanitize=address")
    check_c_compiler_flag("-fsanitize=address" fsanitize-address)
    check_c_compiler_flag("-fsanitize-blacklist=${PROJECT_BINARY_DIR}/foo.txt" fsanitize-blacklist)
    check_c_compiler_flag("-fsanitize-address-use-after-scope" fsanitize-address-use-after-scope)

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=address")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=address")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=address")

    if(fsanitize-blacklist)
      set(CMAKE_C_FLAGS
          "${CMAKE_C_FLAGS} -fsanitize-blacklist=${PROJECT_SOURCE_DIR}/scripts/blacklist-address-sanitizer.txt"
      )
    endif(fsanitize-blacklist)

    if(fsanitize-address-use-after-scope)
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize-address-use-after-scope")
    endif(fsanitize-address-use-after-scope)
  elseif(WITH_SANITIZE_MEMORY)
    list(APPEND CMAKE_REQUIRED_LINK_OPTIONS "-fsanitize=memory")
    check_c_compiler_flag("-fsanitize=memory" fsanitize-memory)
    check_c_compiler_flag("-fsanitize-blacklist=${PROJECT_BINARY_DIR}/foo.txt" fsanitize-blacklist)
    check_c_compiler_flag("-fsanitize-memory-use-after-dtor" fsanitize-memory-use-after-dtor)
    check_c_compiler_flag("-fsanitize-memory-track-origins" fsanitize-memory-track-origins)

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=memory")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=memory")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=memory")

    if(fsanitize-blacklist)
      set(CMAKE_C_FLAGS
          "${CMAKE_C_FLAGS} -fsanitize-blacklist=${PROJECT_SOURCE_DIR}/scripts/blacklist-memory-sanitizer.txt"
      )
    endif(fsanitize-blacklist)

    if(fsanitize-memory-use-after-dtor)
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize-memory-use-after-dtor")
    endif(fsanitize-memory-use-after-dtor)

    if(fsanitize-memory-track-origins)
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize-memory-track-origins")
    endif(fsanitize-memory-track-origins)
  elseif(WITH_SANITIZE_THREAD)
    list(APPEND CMAKE_REQUIRED_LINK_OPTIONS "-fsanitize=thread")
    check_c_compiler_flag("-fsanitize=thread" fsanitize-thread)
    check_c_compiler_flag("-fsanitize-blacklist=${PROJECT_BINARY_DIR}/foo.txt" fsanitize-blacklist)

    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fsanitize=thread")
    if(fsanitize-blacklist)
      set(CMAKE_C_FLAGS
          "${CMAKE_C_FLAGS} -fsanitize-blacklist=${PROJECT_SOURCE_DIR}/scripts/blacklist-thread-sanitizer.txt"
      )
    endif(fsanitize-blacklist)
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=thread")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=thread")
  endif()

  file(REMOVE ${PROJECT_BINARY_DIR}/foo.txt)
  set(CMAKE_REQUIRED_LINK_OPTIONS ${CMAKE_REQUIRED_LINK_OPTIONS_SAVED})

  if(WITH_NO_UNDEFINED)
    check_c_compiler_flag(-Wl,--no-undefined no-undefined)

    if(no-undefined)
      set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--no-undefined")
      set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-undefined")
    endif()
  endif()
endif()

if(MSVC)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /Gd")

  set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR})
  set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR})

  add_compile_options("$<$<CONFIG:Release>:/Zi>")

  add_compile_definitions(_CRT_NONSTDC_NO_DEPRECATE)
endif()

if(ANDROID)
  # workaround for https://github.com/android-ndk/ndk/issues/243
  string(REPLACE "-g " "" CMAKE_C_FLAGS ${CMAKE_C_FLAGS})
  set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g")
endif()

if(WIN32)
  add_compile_definitions(UNICODE _UNICODE)
  add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
  add_compile_definitions(WIN32_LEAN_AND_MEAN)
  add_compile_definitions(_WINSOCK_DEPRECATED_NO_WARNINGS)

  set(CMAKE_DL_LIBS "")
  set(CMAKE_USE_RELATIVE_PATH ON)
  string(TIMESTAMP RC_VERSION_YEAR "%Y")

  if(NOT DEFINED CMAKE_WINDOWS_VERSION)
    set(CMAKE_WINDOWS_VERSION "WIN7")
  endif()

  if(CMAKE_WINDOWS_VERSION STREQUAL "WINXP")
    add_compile_definitions(WINVER=0x0501 _WIN32_WINNT=0x0501)
  elseif(CMAKE_WINDOWS_VERSION STREQUAL "WIN7")
    add_compile_definitions(WINVER=0x0601 _WIN32_WINNT=0x0601)
  elseif(CMAKE_WINDOWS_VERSION STREQUAL "WIN8")
    add_compile_definitions(WINVER=0x0602 _WIN32_WINNT=0x0602)
  elseif(CMAKE_WINDOWS_VERSION STREQUAL "WIN10")
    add_compile_definitions(WINVER=0x0A00 _WIN32_WINNT=0x0A00)
  endif()

  # Set product and vendor for dll and exe version information.
  set(RC_VERSION_VENDOR ${VENDOR})
  set(RC_VERSION_PRODUCT ${PRODUCT})
  set(RC_VERSION_PATCH ${BUILD_NUMBER})
  set(RC_VERSION_DESCRIPTION
      "${FREERDP_VERSION_FULL} ${GIT_REVISION} ${CMAKE_WINDOWS_VERSION} ${CMAKE_SYSTEM_PROCESSOR}"
  )

  if(FREERDP_EXTERNAL_SSL_PATH)
    set(OPENSSL_ROOT_DIR ${FREERDP_EXTERNAL_SSL_PATH})
  endif()
endif()

add_compile_definitions(FREERDP_EXPORTS)

# Mac OS X
if(APPLE)
  if(IOS)
    if(NOT FREERDP_IOS_EXTERNAL_SSL_PATH)
      message(
        STATUS
          "FREERDP_IOS_EXTERNAL_SSL_PATH not set! Required if openssl is not found in the iOS SDK (which usually isn't"
      )
    endif()
    set(CMAKE_FIND_ROOT_PATH ${CMAKE_FIND_ROOT_PATH} ${FREERDP_IOS_EXTERNAL_SSL_PATH})
    set_property(GLOBAL PROPERTY XCODE_ATTRIBUTE_SKIP_INSTALL YES)
  endif(IOS)

  # Temporarily disabled, causes the cmake script to be reexecuted, causing the compilation to fail.
  # Workaround: specify the parameter in the command-line
  #	if(WITH_CLANG)
  #		set(CMAKE_C_COMPILER "clang")
  #	endif()

  if(WITH_VERBOSE)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -v")
  endif()
endif(APPLE)

# Android
if(ANDROID)
  set_property(GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS ${ANDROID_LIBRARY_USE_LIB64_PATHS})

  if(ANDROID_ABI STREQUAL arm64-v8a)
    include(CheckCCompilerFlag)
    check_c_compiler_flag("-mfloat-abi=softfp" ABI_SOFTFP_SUPPORTED)

    if(ABI_SOFTFP_SUPPORTED)
      # https://github.com/android/ndk/issues/910
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mfloat-abi=softfp")
    endif()
  endif()

  add_compile_definitions("$<$<CONFIG:Debug>:NDK_DEBUG=1>")

  # NOTE: Manually add -gdwarf-3, as newer toolchains default to -gdwarf-4,
  #       which is not supported by the gdbserver binary shipped with
  #       the android NDK (tested with r9b)
  add_compile_options("$<$<CONFIG:Debug>:-gdwarf-3>")
  add_link_options(-llog)

  # CMAKE_PREFIX_PATH detection is broken in most Android toolchain files
  # Append it to CMAKE_FIND_ROOT_PATH and avoid potential duplicates
  list(APPEND CMAKE_FIND_ROOT_PATH ${CMAKE_PREFIX_PATH})
  list(REMOVE_DUPLICATES CMAKE_FIND_ROOT_PATH)

  if(NOT FREERDP_EXTERNAL_PATH)
    if(IS_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/external/")
      set(FREERDP_EXTERNAL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/external/")
    else()
      message(STATUS "FREERDP_EXTERNAL_PATH not set!")
    endif()
  endif()

  list(APPEND CMAKE_INCLUDE_PATH ${FREERDP_EXTERNAL_PATH}/${ANDROID_ABI}/include)
  list(APPEND CMAKE_LIBRARY_PATH ${FREERDP_EXTERNAL_PATH}/${ANDROID_ABI}/)
  set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY BOTH)
  set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE BOTH)

  if(WITH_GPROF)
    configure_file(
      ${PROJECT_SOURCE_DIR}/scripts/gprof_generate.sh.in ${PROJECT_BINARY_DIR}/scripts/gprof_generate.sh @ONLY
    )
  endif(WITH_GPROF)
endif()

if(WITH_VALGRIND_MEMCHECK)
  check_include_files(valgrind/memcheck.h FREERDP_HAVE_VALGRIND_MEMCHECK_H)
else()
  unset(FREERDP_HAVE_VALGRIND_MEMCHECK_H CACHE)
endif()

if(UNIX OR CYGWIN)
  set(WAYLAND_FEATURE_TYPE "RECOMMENDED")
else()
  set(WAYLAND_FEATURE_TYPE "DISABLED")
endif()

if(WITH_PCSC_WINPR)
  find_package(PCSCWinPR)
endif()

set(WAYLAND_FEATURE_PURPOSE "Wayland")
set(WAYLAND_FEATURE_DESCRIPTION "Wayland client")

set(OPENSSL_FEATURE_TYPE "REQUIRED")
set(OPENSSL_FEATURE_PURPOSE "cryptography")
set(OPENSSL_FEATURE_DESCRIPTION "encryption, certificate validation, hashing functions")

set(MBEDTLS_FEATURE_TYPE "OPTIONAL")
set(MBEDTLS_FEATURE_PURPOSE "cryptography")
set(MBEDTLS_FEATURE_DESCRIPTION "encryption, certificate validation, hashing functions")

set(PCSC_FEATURE_TYPE "RECOMMENDED")
set(PCSC_FEATURE_PURPOSE "smart card")
set(PCSC_FEATURE_DESCRIPTION "smart card device redirection")

set(OPENH264_FEATURE_TYPE "OPTIONAL")
set(OPENH264_FEATURE_PURPOSE "codec")
set(OPENH264_FEATURE_DESCRIPTION "use OpenH264 library")

set(OPENCL_FEATURE_TYPE "OPTIONAL")
set(OPENCL_FEATURE_PURPOSE "codec")
set(OPENCL_FEATURE_DESCRIPTION "use OpenCL library")

set(GSM_FEATURE_TYPE "OPTIONAL")
set(GSM_FEATURE_PURPOSE "codec")
set(GSM_FEATURE_DESCRIPTION "GSM audio codec library")

set(LAME_FEATURE_TYPE "OPTIONAL")
set(LAME_FEATURE_PURPOSE "codec")
set(LAME_FEATURE_DESCRIPTION "lame MP3 audio codec library")

set(FAAD2_FEATURE_TYPE "OPTIONAL")
set(FAAD2_FEATURE_PURPOSE "codec")
set(FAAD2_FEATURE_DESCRIPTION "FAAD2 AAC audio codec library")

set(FAAC_FEATURE_TYPE "OPTIONAL")
set(FAAC_FEATURE_PURPOSE "codec")
set(FAAC_FEATURE_DESCRIPTION "FAAC AAC audio codec library")

set(SOXR_FEATURE_TYPE "OPTIONAL")
set(SOXR_FEATURE_PURPOSE "codec")
set(SOXR_FEATURE_DESCRIPTION "SOX audio resample library")

if(WIN32)
  set(WAYLAND_FEATURE_TYPE "DISABLED")
  set(PCSC_FEATURE_TYPE "DISABLED")
endif()

if(APPLE)
  set(WAYLAND_FEATURE_TYPE "DISABLED")
  if(IOS)
    set(PCSC_FEATURE_TYPE "DISABLED")
  endif()
endif()

if(ANDROID)
  set(WAYLAND_FEATURE_TYPE "DISABLED")
  set(PCSC_FEATURE_TYPE "DISABLED")
endif()

find_feature(Wayland ${WAYLAND_FEATURE_TYPE} ${WAYLAND_FEATURE_PURPOSE} ${WAYLAND_FEATURE_DESCRIPTION})

option(WITH_LIBRESSL "build with LibreSSL" OFF)
if(WITH_LIBRESSL)
  find_package(LibreSSL REQUIRED)
  include_directories(SYSTEM ${LibreSSL_INCLUDE_DIRS})
  set(OPENSSL_INCLUDE_DIR ${LIBRESSL_INCLUDE_DIR})
  set(OPENSSL_LIBRARIES ${LIBRESSL_LIBRARIES})
  set(OPENSSL_CRYPTO_LIBRARIES ${LIBRESSL_LIBRARIES})
  set(WITH_OPENSSL ON)
  set(OPENSSL_FOUND ON)
  add_compile_definitions("WITH_LIBRESSL")
  add_compile_definitions("WITH_OPENSSL")
else()
  find_feature(OpenSSL ${OPENSSL_FEATURE_TYPE} ${OPENSSL_FEATURE_PURPOSE} ${OPENSSL_FEATURE_DESCRIPTION})
  find_feature(MbedTLS ${MBEDTLS_FEATURE_TYPE} ${MBEDTLS_FEATURE_PURPOSE} ${MBEDTLS_FEATURE_DESCRIPTION})
endif()

find_feature(PCSC ${PCSC_FEATURE_TYPE} ${PCSC_FEATURE_PURPOSE} ${PCSC_FEATURE_DESCRIPTION})

if(WITH_DSP_FFMPEG OR WITH_VIDEO_FFMPEG OR WITH_FFMPEG)
  find_package(FFmpeg REQUIRED COMPONENTS AVUTIL AVCODEC)
endif()

find_feature(OpenH264 ${OPENH264_FEATURE_TYPE} ${OPENH264_FEATURE_PURPOSE} ${OPENH264_FEATURE_DESCRIPTION})
find_feature(OpenCL ${OPENCL_FEATURE_TYPE} ${OPENCL_FEATURE_PURPOSE} ${OPENCL_FEATURE_DESCRIPTION})
find_feature(GSM ${GSM_FEATURE_TYPE} ${GSM_FEATURE_PURPOSE} ${GSM_FEATURE_DESCRIPTION})
find_feature(LAME ${LAME_FEATURE_TYPE} ${LAME_FEATURE_PURPOSE} ${LAME_FEATURE_DESCRIPTION})
find_feature(FAAD2 ${FAAD2_FEATURE_TYPE} ${FAAD2_FEATURE_PURPOSE} ${FAAD2_FEATURE_DESCRIPTION})
find_feature(FAAC ${FAAC_FEATURE_TYPE} ${FAAC_FEATURE_PURPOSE} ${FAAC_FEATURE_DESCRIPTION})
find_feature(soxr ${SOXR_FEATURE_TYPE} ${SOXR_FEATURE_PURPOSE} ${SOXR_FEATURE_DESCRIPTION})

if(WITH_OPENH264 AND NOT WITH_OPENH264_LOADING)
  option(WITH_OPENH264_LOADING "Use LoadLibrary to load openh264 at runtime" OFF)
endif(WITH_OPENH264 AND NOT WITH_OPENH264_LOADING)

# Version check, if we have detected FFMPEG but the version is too old
# deactivate it as sound backend.
if(WITH_DSP_FFMPEG)
  if(AVCODEC_VERSION VERSION_LESS "57.48.101")
    message(
      WARNING
        "FFmpeg version detected (${AVCODEC_VERSION}) is too old. (Require at least 57.48.101 for sound). Deactivating"
    )
    set(WITH_DSP_FFMPEG OFF)
  endif()
endif(WITH_DSP_FFMPEG)

if(WITH_OPENH264 AND NOT OPENH264_FOUND)
  message(FATAL_ERROR "OpenH264 support requested but not detected")
endif()
set(WITH_OPENH264 ${OPENH264_FOUND})

if(OPENSSL_FOUND)
  add_compile_definitions("WITH_OPENSSL")
  message(STATUS "Using OpenSSL Version: ${OPENSSL_VERSION}")
  include_directories(SYSTEM ${OPENSSL_INCLUDE_DIR})
endif()

if(MBEDTLS_FOUND)
  add_compile_definitions("WITH_MBEDTLS")
endif()

if(WITH_OPENH264 OR WITH_MEDIA_FOUNDATION OR WITH_VIDEO_FFMPEG OR WITH_MEDIACODEC)
  set(WITH_GFX_H264 ON)
else()
  set(WITH_GFX_H264 OFF)
endif()

# Android expects all libraries to be loadable
# without paths.
if(ANDROID OR WIN32 OR MAC_BUNDLE)
  set(PLUGIN_ABS_PATHS_DEFAULT OFF)
else()
  set(PLUGIN_ABS_PATHS_DEFAULT ON)
endif()
option(WITH_ABSOLUTE_PLUGIN_LOAD_PATHS "Load plugins with absolute paths" ${PLUGIN_ABS_PATHS_DEFAULT})

if(NOT WITH_ABSOLUTE_PLUGIN_LOAD_PATHS)
  set(FREERDP_DATA_PATH "share")
  if(NOT FREERDP_INSTALL_PREFIX)
    set(FREERDP_INSTALL_PREFIX ".")
  endif()
  set(FREERDP_LIBRARY_PATH ".")
  set(FREERDP_PLUGIN_PATH ".")
else()
  set(FREERDP_DATA_PATH "${CMAKE_INSTALL_PREFIX}/share/${FREERDP_MAJOR_DIR}")
  if(NOT FREERDP_INSTALL_PREFIX)
    set(FREERDP_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
  endif()
  set(FREERDP_LIBRARY_PATH "${CMAKE_INSTALL_LIBDIR}")
  if(WIN32)
    set(FREERDP_PLUGIN_PATH "${CMAKE_INSTALL_BINDIR}/${FREERDP_MAJOR_DIR}")
  else()
    set(FREERDP_PLUGIN_PATH "${CMAKE_INSTALL_LIBDIR}/${FREERDP_MAJOR_DIR}")
  endif()
endif()
set(FREERDP_ADDIN_PATH "${FREERDP_PLUGIN_PATH}")

# Path to put extensions
set(FREERDP_EXTENSION_POSTFIX "${FREERDP_MAJOR_DIR}/extensions")
set(FREERDP_EXTENSION_REL_PATH "${CMAKE_INSTALL_LIBDIR}/${FREERDP_EXTENSION_POSTFIX}")
set(FREERDP_EXTENSION_PATH "${CMAKE_INSTALL_FULL_LIBDIR}/${FREERDP_EXTENSION_POSTFIX}")

# Proxy plugins path
if(NOT DEFINED PROXY_PLUGINDIR)
  message("using default plugins location")
  set(FREERDP_PROXY_PLUGINDIR "${FREERDP_PLUGIN_PATH}/proxy/")
else()
  set(FREERDP_PROXY_PLUGINDIR "${PROXY_PLUGINDIR}")
endif()

# Android profiling
if(ANDROID)
  if(WITH_GPROF)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -pg")
    set(PROFILER_LIBRARIES "${FREERDP_EXTERNAL_PROFILER_PATH}/obj/local/${ANDROID_ABI}/libandroid-ndk-profiler.a")
    include_directories(SYSTEM "${FREERDP_EXTERNAL_PROFILER_PATH}")
  endif()
endif()

# Unit Tests

include(CTest)

if(BUILD_TESTING_INTERNAL OR BUILD_TESTING)
  enable_testing()

  if(MSVC)
    set(TESTING_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
  else()
    set(TESTING_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/Testing")
  endif()
endif()

include(CommonConfigOptions)

if(FREERDP_UNIFIED_BUILD)
  add_subdirectory(winpr)
  if(WITH_WAYLAND)
    add_subdirectory(uwac)
  endif()
  if(WITH_SERVER)
    option(WITH_RDTK "build rdtk toolkit" ON)
    if(WITH_RDTK)
      add_subdirectory(rdtk)
    endif()
  endif()

  include_directories(${PROJECT_SOURCE_DIR}/winpr/include)
  include_directories(${PROJECT_BINARY_DIR}/winpr/include)
else()
  find_package(WinPR 3 REQUIRED)
  include_directories(SYSTEM ${WinPR_INCLUDE_DIR})
endif()

option(WITH_AAD "Compile with support for Azure AD authentication" ${WITH_WINPR_JSON})

# Include directories
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Sub-directories

if(WITH_THIRD_PARTY)
  add_subdirectory(third-party)
  if(NOT "${THIRD_PARTY_INCLUDES}" STREQUAL "")
    include_directories(SYSTEM ${THIRD_PARTY_INCLUDES})
  endif()
endif()

add_subdirectory(libfreerdp)

if(WITH_CHANNELS)
  add_subdirectory(channels)
endif()

if(WITH_CLIENT_COMMON OR WITH_CLIENT)
  add_subdirectory(client)
endif()

if(WITH_SERVER)
  add_subdirectory(server)
endif()

# Packaging

set(CMAKE_CPACK_INCLUDE_FILE "CMakeCPack.cmake")

if(NOT (VENDOR MATCHES "FreeRDP"))
  if(DEFINED CLIENT_VENDOR_PATH)
    if(EXISTS "${PROJECT_SOURCE_DIR}/${CLIENT_VENDOR_PATH}/CMakeCPack.cmake")
      set(CMAKE_CPACK_INCLUDE_FILE "${CLIENT_VENDOR_PATH}/CMakeCPack.cmake")
    endif()
  endif()
endif()

#message("VENDOR: ${VENDOR} CLIENT_VENDOR_PATH: ${CLIENT_VENDOR_PATH} CMAKE_CPACK_INCLUDE_FILE: ${CMAKE_CPACK_INCLUDE_FILE}")

set(FREERDP_BUILD_CONFIG_LIST "")
get_cmake_property(res VARIABLES)
foreach(var ${res})
  if(var MATCHES "^WITH_*|^BUILD_TESTING*|^WINPR_HAVE_*")
    list(APPEND FREERDP_BUILD_CONFIG_LIST "${var}=${${var}}")
  endif()
endforeach()
string(REPLACE ";" " " FREERDP_BUILD_CONFIG "${FREERDP_BUILD_CONFIG_LIST}")

add_subdirectory(include)

include(${CMAKE_CPACK_INCLUDE_FILE})

message(STATUS "Intrinsic path configuration:")
#ShowCMakeVars("^CMAKE_INSTALL_PREFIX")
#ShowCMakeVars("^CMAKE_INSTALL_LIBDIR")
showcmakevars("^FREERDP_INSTALL_PREFIX|^FREERDP_LIBRARY_PATH|^FREERDP_PLUGIN_PATH")
showcmakevars("^FREERDP_ADDIN_PATH|^FREERDP_EXTENSION_PATH|^FREERDP_PROXY_PLUGINDIR")
