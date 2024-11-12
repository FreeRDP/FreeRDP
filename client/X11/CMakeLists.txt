# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP X11 Client
#
# Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
# Copyright 2013 Corey Clayton <can.of.tuna@gmail.com>
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

if(NOT FREERDP_DEFAULT_PROJECT_VERSION)
  set(FREERDP_DEFAULT_PROJECT_VERSION "1.0.0.0")
endif()

project(xfreerdp-client LANGUAGES C VERSION ${FREERDP_DEFAULT_PROJECT_VERSION})

message("project ${PROJECT_NAME} is using version ${PROJECT_VERSION}")

set(MODULE_NAME "xfreerdp")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/)
include(CommonConfigOptions)

include(ConfigureFreeRDP)

find_package(X11 REQUIRED)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../../resources)
include_directories(SYSTEM ${X11_INCLUDE_DIRS})
include_directories(SYSTEM ${OPENSSL_INCLUDE_DIR})

set(SRCS
    xf_utils.h
    xf_utils.c
    xf_gfx.c
    xf_gfx.h
    xf_rail.c
    xf_rail.h
    xf_input.c
    xf_input.h
    xf_event.c
    xf_event.h
    xf_floatbar.c
    xf_floatbar.h
    xf_input.c
    xf_input.h
    xf_channels.c
    xf_channels.h
    xf_cliprdr.c
    xf_cliprdr.h
    xf_monitor.c
    xf_monitor.h
    xf_disp.c
    xf_disp.h
    xf_graphics.c
    xf_graphics.h
    xf_keyboard.c
    xf_keyboard.h
    xf_video.c
    xf_video.h
    xf_window.c
    xf_window.h
    xf_client.c
    xf_client.h
)

if(CHANNEL_TSMF_CLIENT)
  list(APPEND SRCS xf_tsmf.c xf_tsmf.h)
endif()

if(CLIENT_INTERFACE_SHARED)
  addtargetwithresourcefile(${PROJECT_NAME} "SHARED" "${PROJECT_VERSION}" SRCS)
else()
  addtargetwithresourcefile(${PROJECT_NAME} "STATIC" "${PROJECT_VERSION}" SRCS)
endif()
target_include_directories(${PROJECT_NAME} INTERFACE $<INSTALL_INTERFACE:include>)

set(PRIV_LIBS ${X11_LIBRARIES})

find_package(X11 REQUIRED)
if(X11_XShm_FOUND)
  add_compile_definitions(WITH_XSHM)
  include_directories(SYSTEM ${X11_XShm_INCLUDE_PATH})
  list(APPEND PRIV_LIBS ${X11_Xext_LIB})
endif()

option(WITH_XINERAMA "[X11] enable xinerama" ON)
if(WITH_XINERAMA)
  find_package(X11 REQUIRED)
  if(X11_Xinerama_FOUND)
    add_compile_definitions(WITH_XINERAMA)
    include_directories(SYSTEM ${X11_Xinerama_INCLUDE_PATH})
    list(APPEND PRIV_LIBS ${X11_Xinerama_LIB})
  endif()
endif()

option(WITH_XEXT "[X11] enable Xext" ON)
if(WITH_XEXT)
  find_package(X11 REQUIRED)
  if(X11_Xext_FOUND)
    add_compile_definitions(WITH_XEXT)
    list(APPEND PRIV_LIBS ${X11_Xext_LIB})
  endif()
endif()

option(WITH_XCURSOR "[X11] enable Xcursor" ON)
if(WITH_XCURSOR)
  find_package(X11 REQUIRED)
  if(X11_Xcursor_FOUND)
    add_compile_definitions(WITH_XCURSOR)
    include_directories(SYSTEM ${X11_Xcursor_INCLUDE_PATH})
    list(APPEND PRIV_LIBS ${X11_Xcursor_LIB})
  endif()
endif()

option(WITH_XV "[X11] enable Xv" ON)
if(WITH_XV)
  find_package(X11 REQUIRED)
  if(X11_Xv_FOUND)
    add_compile_definitions(WITH_XV)
    include_directories(SYSTEM ${X11_Xv_INCLUDE_PATH})
    list(APPEND PRIV_LIBS ${X11_Xv_LIB})
  endif()
endif()

option(WITH_XI "[X11] enable Xi" ON)
if(WITH_XI)
  find_package(X11 REQUIRED)
  if(X11_Xi_FOUND)
    add_compile_definitions(WITH_XI)
    include_directories(SYSTEM ${X11_Xi_INCLUDE_PATH})
    list(APPEND PRIV_LIBS ${X11_Xi_LIB})
  endif()
endif()

option(WITH_XRENDER "[X11] enable XRender" ON)
if(WITH_XRENDER)
  find_package(X11 REQUIRED)
  if(X11_Xrender_FOUND)
    add_compile_definitions(WITH_XRENDER)
    include_directories(SYSTEM ${X11_Xrender_INCLUDE_PATH})
    list(APPEND PRIV_LIBS ${X11_Xrender_LIB})
  endif()
endif()

option(WITH_XRANDR "[X11] enable XRandR" ON)
if(WITH_XRANDR)
  find_package(X11 REQUIRED)
  if(X11_Xrandr_FOUND)
    add_compile_definitions(WITH_XRANDR)
    include_directories(SYSTEM ${X11_Xrandr_INCLUDE_PATH})
    list(APPEND PRIV_LIBS ${X11_Xrandr_LIB})
  endif()
endif()

option(WITH_XFIXES "[X11] enable Xfixes" ON)
if(WITH_XFIXES)
  find_package(X11 REQUIRED)
  if(X11_Xfixes_FOUND)
    add_compile_definitions(WITH_XFIXES)
    include_directories(SYSTEM ${X11_Xfixes_INCLUDE_PATH})
    list(APPEND PRIV_LIBS ${X11_Xfixes_LIB})
  endif()
endif()

list(APPEND PUB_LIBS freerdp-client)

list(APPEND PRIV_LIBS m)
if(NOT APPLE)
  list(APPEND PRIV_LIBS rt)
endif()
target_link_libraries(${PROJECT_NAME} PUBLIC ${PUB_LIBS})
target_link_libraries(${PROJECT_NAME} PRIVATE ${PRIV_LIBS})

if(WITH_CLIENT_INTERFACE)
  install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT libraries)
endif()
add_subdirectory(cli)
add_subdirectory(man)

set_property(TARGET ${PROJECT_NAME} PROPERTY FOLDER "Client/X11")
