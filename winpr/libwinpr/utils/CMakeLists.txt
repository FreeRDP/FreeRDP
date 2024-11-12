# WinPR: Windows Portable Runtime
# libwinpr-utils cmake build script
#
# Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

include(CheckFunctionExists)
include(CMakeDependentOption)

winpr_include_directory_add(${CMAKE_CURRENT_SOURCE_DIR})

option(WITH_LODEPNG "build WinPR with PNG support" OFF)
if(WITH_LODEPNG)
  find_package(lodepng REQUIRED)

  winpr_definition_add(WITH_LODEPNG)
  set(WINPR_WITH_PNG ON CACHE BOOL "build cache")

  winpr_include_directory_add(${lodepng_INCLUDE_DIRS})
  winpr_library_add_private(${lodepng_LIBRARIES})
endif()

option(WINPR_UTILS_IMAGE_PNG "Add PNG <--> BMP conversion support to clipboard" OFF)
if(WINPR_UTILS_IMAGE_PNG)
  find_package(PNG REQUIRED)

  set(WINPR_WITH_PNG ON CACHE BOOL "build cache")
  winpr_include_directory_add(${PNG_INCLUDE_DIRS})
  winpr_library_add_private(${PNG_LIBRARIES})
endif()

option(WINPR_UTILS_IMAGE_WEBP "Add WebP <--> BMP conversion support to clipboard" OFF)
if(WINPR_UTILS_IMAGE_WEBP)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(WEBP libwebp REQUIRED)

  winpr_include_directory_add(${WEBP_INCLUDE_DIRS})
  winpr_library_add_private(${WEBP_LIBRARIES})
endif()

option(WINPR_UTILS_IMAGE_JPEG "Add Jpeg <--> BMP conversion support to clipboard" OFF)
if(WINPR_UTILS_IMAGE_JPEG)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(JPEG libjpeg REQUIRED)

  winpr_include_directory_add(${JPEG_INCLUDE_DIRS})
  winpr_library_add_private(${JPEG_LIBRARIES})
endif()

set(COLLECTIONS_SRCS
    collections/Object.c
    collections/Queue.c
    collections/Stack.c
    collections/PubSub.c
    collections/BitStream.c
    collections/ArrayList.c
    collections/LinkedList.c
    collections/HashTable.c
    collections/ListDictionary.c
    collections/CountdownEvent.c
    collections/BufferPool.c
    collections/ObjectPool.c
    collections/StreamPool.c
    collections/MessageQueue.c
    collections/MessagePipe.c
)

if(WINPR_HAVE_SYSLOG_H)
  set(SYSLOG_SRCS wlog/SyslogAppender.c wlog/SyslogAppender.h)
endif()

find_package(libsystemd)
option(WITH_SYSTEMD "allows to export wLog to systemd journal" ${libsystemd_FOUND})
if(WITH_LIBSYSTEMD)
  find_package(libsystemd REQUIRED)
  set(WINPR_HAVE_JOURNALD_H TRUE)
  set(JOURNALD_SRCS wlog/JournaldAppender.c wlog/JournaldAppender.h)
  winpr_include_directory_add(${LIBSYSTEMD_INCLUDE_DIR})
  winpr_library_add_private(${LIBSYSTEMD_LIBRARY})
else()
  unset(WINPR_HAVE_JOURNALD_H)
endif()

set(WLOG_SRCS
    wlog/wlog.c
    wlog/wlog.h
    wlog/Layout.c
    wlog/Layout.h
    wlog/Message.c
    wlog/Message.h
    wlog/DataMessage.c
    wlog/DataMessage.h
    wlog/ImageMessage.c
    wlog/ImageMessage.h
    wlog/PacketMessage.c
    wlog/PacketMessage.h
    wlog/Appender.c
    wlog/Appender.h
    wlog/FileAppender.c
    wlog/FileAppender.h
    wlog/BinaryAppender.c
    wlog/BinaryAppender.h
    wlog/CallbackAppender.c
    wlog/CallbackAppender.h
    wlog/ConsoleAppender.c
    wlog/ConsoleAppender.h
    wlog/UdpAppender.c
    wlog/UdpAppender.h
    ${SYSLOG_SRCS}
    ${JOURNALD_SRCS}
)

set(ASN1_SRCS asn1/asn1.c)

set(SRCS
    ini.c
    sam.c
    ntlm.c
    image.c
    print.c
    stream.h
    stream.c
    strlst.c
    debug.c
    winpr.c
    cmdline.c
    ssl.c
)

if(ANDROID)
  list(APPEND SRCS android.h android.c)
  include_directories(${CMAKE_CURRENT_SOURCE_DIR})
  if(NOT WINPR_HAVE_UNWIND_H)
    message("[backtrace] android NDK without unwind.h, falling back to corkscrew")
    set(WINPR_HAVE_CORKSCREW 1)
  endif()
endif()

if(WINPR_HAVE_CORKSCREW)
  list(APPEND SRCS corkscrew/debug.c corkscrew/debug.h)
endif()

if(WIN32)
  list(APPEND SRCS windows/debug.c windows/debug.h)
endif()

if(WINPR_HAVE_EXECINFO_H)
  option(USE_EXECINFO "Use execinfo.h to generate backtraces" ON)
  if(USE_EXECINFO)
    winpr_definition_add(USE_EXECINFO)
    list(APPEND SRCS execinfo/debug.c execinfo/debug.h)
  endif()
endif()

if(WINPR_HAVE_UNWIND_H)
  option(USE_UNWIND "Use unwind.h to generate backtraces" ON)
  if(USE_UNWIND)
    winpr_definition_add(USE_UNWIND)
    list(APPEND SRCS unwind/debug.c unwind/debug.h)
  endif()
endif()

include(JsonDetect)
if(NOT WITH_JSON_DISABLED)
  if(JSONC_FOUND AND NOT WITH_CJSON_REQUIRED)
    winpr_library_add_private(${JSONC_LIBRARIES})
    winpr_include_directory_add(${JSONC_INCLUDE_DIRS})
    winpr_definition_add(WITH_JSONC)
    set(WITH_WINPR_JSON ON CACHE INTERNAL "internal")
  elseif(CJSON_FOUND)
    winpr_library_add_private(${CJSON_LIBRARIES})
    winpr_include_directory_add(${CJSON_INCLUDE_DIRS})
    winpr_definition_add(WITH_CJSON)
    set(WITH_WINPR_JSON ON CACHE INTERNAL "internal")
  endif()
endif()

winpr_module_add(json/json.c)

winpr_module_add(${SRCS} ${COLLECTIONS_SRCS} ${WLOG_SRCS} ${ASN1_SRCS})

winpr_include_directory_add(".")

if(OPENSSL_FOUND)
  winpr_include_directory_add(${OPENSSL_INCLUDE_DIR})
  winpr_library_add_private(${OPENSSL_LIBRARIES})
endif()

if(MBEDTLS_FOUND)
  winpr_include_directory_add(${MBEDTLS_INCLUDE_DIR})
  winpr_library_add_private(${MBEDTLS_LIBRARIES})
endif()

if(UNIX)
  winpr_library_add_private(m)

  set(CMAKE_REQUIRED_INCLUDES backtrace.h)
  check_function_exists(backtrace BACKTRACE)
  if(NOT BACKTRACE)
    set(CMAKE_REQUIRED_LIBRARIES execinfo)
    check_function_exists(backtrace EXECINFO)
    if(EXECINFO)
      winpr_library_add_private(execinfo)
    endif()
  endif()
endif()

if(WIN32)
  winpr_library_add_public(dbghelp)
endif()

if(BUILD_TESTING_INTERNAL OR BUILD_TESTING)
  add_subdirectory(test)
endif()
