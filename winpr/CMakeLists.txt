# WinPR: Windows Portable Runtime
# winpr cmake build script
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

cmake_minimum_required(VERSION 2.8)

project(WinPR C)

set(CMAKE_COLOR_MAKEFILE ON)

if(FREERDP_VERSION)
	set(FREERDP_BUILD)
endif()

# Include cmake modules
include(CheckIncludeFiles)
include(CheckLibraryExists)
include(CheckStructHasMember)
include(FindPkgConfig)
include(TestBigEndian)

# Include our extra modules
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/)

# Check for cmake compatibility (enable/disable features)
include(CheckCmakeCompat)

include(FindFeature)
include(AutoVersioning)
include(ConfigOptions)
include(CheckCCompilerFlag)
include(GNUInstallDirsWrapper)

# Soname versioning
set(WINPR_VERSION_MAJOR "0")
set(WINPR_VERSION_MINOR "1")
set(WINPR_VERSION_REVISION "0")
set(WINPR_VERSION "${WINPR_VERSION_MAJOR}.${WINPR_VERSION_MINOR}")
set(WINPR_VERSION_FULL "${WINPR_VERSION}.${WINPR_VERSION_REVISION}")

# Default to release build type
if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE "Release")
endif()

# Default to build shared libs
if(NOT DEFINED BUILD_SHARED_LIBS)
	set(BUILD_SHARED_LIBS ON)
endif()

if(FREERDP_BUILD)
	include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include PARENT_SCOPE)
	include_directories(${CMAKE_CURRENT_BINARY_DIR}/include PARENT_SCOPE)
else()
	include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
	include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/winpr/config.h.in 
	${CMAKE_CURRENT_BINARY_DIR}/include/winpr/config.h)

add_subdirectory(include)

add_subdirectory(libwinpr)

if(NOT ANDROID AND NOT IOS)
	add_subdirectory(tools)
endif()
