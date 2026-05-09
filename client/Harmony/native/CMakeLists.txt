cmake_minimum_required(VERSION 3.16)
project(freerdp_harmony_native LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(FREERDP_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../..)

if(CMAKE_SYSTEM_NAME STREQUAL "OHOS")
  list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")

  set(FREERDP_SOURCE_DIR ${FREERDP_ROOT_DIR})
  include(ExternalOpenSSL)
  include(ExternalFreeRDP)

  add_library(freerdp_harmony_bridge SHARED
    freerdp_adapter.cpp
    freerdp_session.cpp
    napi_export.cpp
  )

  target_include_directories(freerdp_harmony_bridge PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${FREERDP_SOURCE_DIR}/include
    ${FREERDP_SOURCE_DIR}/winpr/include
    ${FREERDP_INSTALL_DIR}/include
    ${FREERDP_INSTALL_DIR}/include/freerdp3
    ${FREERDP_INSTALL_DIR}/include/winpr3
  )

  target_link_libraries(freerdp_harmony_bridge PRIVATE
    openssl-lib
    libace_napi.z.so
    libhilog_ndk.z.so
    libnative_window.so
    libnative_buffer.so
    z
  )

  target_link_options(freerdp_harmony_bridge PRIVATE
    "-Wl,--whole-archive"
    "SHELL:@${FREERDP_INSTALL_DIR}/lib/freerdp_archives.rsp"
    "-Wl,--no-whole-archive"
  )

  add_dependencies(freerdp_harmony_bridge freerdp)

else()
  set(FREERDP_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/freerdp-root-build)

  set(WITH_CLIENT_HARMONY OFF CACHE BOOL "" FORCE)
  set(WITH_CLIENT ON CACHE BOOL "" FORCE)
  set(WITH_CLIENT_COMMON ON CACHE BOOL "" FORCE)
  set(WITH_CLIENT_CHANNELS ON CACHE BOOL "" FORCE)
  set(WITH_SERVER OFF CACHE BOOL "" FORCE)
  set(WITH_SAMPLE OFF CACHE BOOL "" FORCE)
  set(WITH_CLIENT_SDL OFF CACHE BOOL "" FORCE)
  set(WITH_X11 OFF CACHE BOOL "" FORCE)
  set(WITH_WAYLAND OFF CACHE BOOL "" FORCE)
  set(WITH_MANPAGES OFF CACHE BOOL "" FORCE)
  set(WITH_PULSE OFF CACHE BOOL "" FORCE)
  set(WITH_CUPS OFF CACHE BOOL "" FORCE)
  set(WITH_FFMPEG OFF CACHE BOOL "" FORCE)
  set(WITH_DSP_FFMPEG OFF CACHE BOOL "" FORCE)
  set(WITH_VIDEO_FFMPEG OFF CACHE BOOL "" FORCE)
  set(WITH_SWSCALE OFF CACHE BOOL "" FORCE)
  set(WITH_KRB5 OFF CACHE BOOL "" FORCE)
  set(WITH_UNICODE_BUILTIN ON CACHE BOOL "" FORCE)
  set(WITH_FUSE OFF CACHE BOOL "" FORCE)
  set(CHANNEL_URBDRC OFF CACHE BOOL "" FORCE)
  set(CHANNEL_URBDRC_CLIENT OFF CACHE BOOL "" FORCE)
  set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

  add_subdirectory(${FREERDP_ROOT_DIR} ${FREERDP_BINARY_DIR} EXCLUDE_FROM_ALL)

  add_library(freerdp_harmony_bridge SHARED
    freerdp_adapter.cpp
    freerdp_session.cpp
    napi_export.cpp
  )

  target_include_directories(freerdp_harmony_bridge PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${FREERDP_ROOT_DIR}/include
    ${FREERDP_ROOT_DIR}/winpr/include
    ${FREERDP_BINARY_DIR}
    ${FREERDP_BINARY_DIR}/include
    ${FREERDP_BINARY_DIR}/winpr/include
  )

  target_link_libraries(freerdp_harmony_bridge PRIVATE
    freerdp-client
    freerdp
    winpr
  )
endif()
