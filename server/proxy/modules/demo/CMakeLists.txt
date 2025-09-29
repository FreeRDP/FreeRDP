#
# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP Proxy Server Demo C++ Module
#
# Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
# Copyright 2021 Armin Novak <anovak@thincast.com>
# Copyright 2021 Thincast Technologies GmbH
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
#

# cmake_minimum_required(VERSION 3.13)

# if(POLICY CMP0091)
#   cmake_policy(SET CMP0091 NEW)
# endif()
# if(NOT FREERDP_DEFAULT_PROJECT_VERSION)
#   set(FREERDP_DEFAULT_PROJECT_VERSION "1.0.0.0")
# endif()

# project(proxy-demo-plugin VERSION ${FREERDP_DEFAULT_PROJECT_VERSION} LANGUAGES CXX)

# message("project ${PROJECT_NAME} is using version ${PROJECT_VERSION}")

# set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../../../cmake/)
# include(ProjectCXXStandard)
# include(CommonConfigOptions)
# include(CXXCompilerFlags)

# set(SRCS demo.cpp)
# addtargetwithresourcefile(${PROJECT_NAME} FALSE "${PROJECT_VERSION}" SRCS FALSE)

# target_compile_definitions(${PROJECT_NAME} PRIVATE $<$<BOOL:${BUILD_SHARED_LIBS}>:BUILD_SHARED_LIBS>)
# target_link_libraries(${PROJECT_NAME} winpr)

# installwithrpath(TARGETS ${PROJECT_NAME} DESTINATION ${FREERDP_PROXY_PLUGINDIR})

# set(PROJECT_PC_REQUIRES_PRIVATE "winpr${FREERDP_API_VERSION}")
# include(ProxyModuleConfig)
# generate_proxy_module_config()


cmake_minimum_required(VERSION 3.13)

if(POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW)
endif()
if(NOT FREERDP_DEFAULT_PROJECT_VERSION)
  set(FREERDP_DEFAULT_PROJECT_VERSION "1.0.0.0")
endif()

project(proxy-demo-plugin VERSION ${FREERDP_DEFAULT_PROJECT_VERSION} LANGUAGES C CXX)

message("project ${PROJECT_NAME} is using version ${PROJECT_VERSION}")

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../../../cmake/)
include(ProjectCXXStandard)
include(CommonConfigOptions)
include(CXXCompilerFlags)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED libavcodec libavformat libavutil libswscale)

if(NOT FFMPEG_FOUND)
  message(FATAL_ERROR "FFmpeg libraries not found: require libavcodec, libavformat, libavutil, libswscale")
else()
  message(STATUS "FFmpeg found: includes=${FFMPEG_INCLUDE_DIRS} libs=${FFMPEG_LIBRARIES}")
endif()

# Your sources
set(SRCS
  demo.cpp
  qrcodegen.c
)

# Ensure qrcodegen.c is compiled as C (not C++)
set_source_files_properties(qrcodegen.c PROPERTIES LANGUAGE C)

# Build the plugin shared lib (FreeRDP helper macro)
addtargetwithresourcefile(${PROJECT_NAME} FALSE "${PROJECT_VERSION}" SRCS FALSE)

# Include current dir so #include "qrcodegen.h" works
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Link against winpr (same as original); add others if you use them
target_compile_definitions(${PROJECT_NAME} PRIVATE $<$<BOOL:${BUILD_SHARED_LIBS}>:BUILD_SHARED_LIBS>)
target_link_libraries(${PROJECT_NAME} PRIVATE winpr)

# Add FFmpeg include dirs and link libs
target_include_directories(${PROJECT_NAME} PRIVATE ${FFMPEG_INCLUDE_DIRS})
# Ensure library search paths are available (optional)
if(FFMPEG_LIBRARY_DIRS)
  link_directories(${FFMPEG_LIBRARY_DIRS})
endif()
target_link_libraries(${PROJECT_NAME} PRIVATE ${FFMPEG_LIBRARIES})
# If pkg-config exposes extra CFLAGS, apply them
if(FFMPEG_CFLAGS)
  target_compile_options(${PROJECT_NAME} PRIVATE ${FFMPEG_CFLAGS})
endif()

# Install to the FreeRDP proxy plugin dir
installwithrpath(TARGETS ${PROJECT_NAME} DESTINATION ${FREERDP_PROXY_PLUGINDIR})

# .pc generation (unchanged)
set(PROJECT_PC_REQUIRES_PRIVATE "winpr${FREERDP_API_VERSION}")
include(ProxyModuleConfig)
generate_proxy_module_config() 
