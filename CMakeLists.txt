# FreeRDP: A Remote Desktop Protocol Client
# FreeRDP cmake build script
#
# Copyright 2011 O.S. Systems Software Ltda.
# Copyright 2011 Otavio Salvador <otavio@ossystems.com.br>
# Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

cmake_minimum_required(VERSION 2.6)
project(FreeRDP C)
set(CMAKE_COLOR_MAKEFILE ON)

# Include cmake modules
include(CheckIncludeFiles)
include(CheckLibraryExists)
include(FindPkgConfig)
include(TestBigEndian)

# Include our extra modules
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/)

include(AutoVersioning)
include(ConfigOptions)

# Soname versioning - 0.0.0 since it is not being managed yet
set(FREERDP_VERSION_MAJOR "0")
set(FREERDP_VERSION_MINOR "0")
set(FREERDP_VERSION_PATCH "0")
set(FREERDP_VERSION "${FREERDP_VERSION_MAJOR}.${FREERDP_VERSION_MINOR}")
set(FREERDP_VERSION_FULL "${FREERDP_VERSION}.${FREERDP_VERSION_PATCH}")

# Default to release build type
if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE "Release")
endif()

# Compiler-specific flags
if(CMAKE_COMPILER_IS_GNUCC)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wno-unused-but-set-variable")
	if(CMAKE_BUILD_TYPE EQUAL "Release")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2")
	else()
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O0")
	endif()
	if(WITH_SSE2)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse2")
	endif()
endif()

if(MSVC)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_X86_")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_X86_ -D_CRT_SECURE_NO_WARNINGS")
endif()

# Include files
check_include_files(sys/param.h HAVE_SYS_PARAM_H)
check_include_files(sys/socket.h HAVE_SYS_SOCKET_H)
check_include_files(netdb.h HAVE_NETDB_H)
check_include_files(fcntl.h HAVE_FCNTL_H)
check_include_files(unistd.h HAVE_UNISTD_H)

# Libraries that we have a hard dependency on
find_package(OpenSSL REQUIRED)

if(NOT WIN32)
	find_package(ZLIB REQUIRED)
	find_package(ALSA)
	find_package(PulseAudio)
endif()

# Endian
test_big_endian(BIG_ENDIAN)

# Path to put keymaps
set(FREERDP_KEYMAP_PATH "${CMAKE_INSTALL_PREFIX}/freerdp/keymaps")

# Path to put plugins
set(FREERDP_PLUGIN_PATH "${CMAKE_INSTALL_PREFIX}/lib/freerdp")

# Include directories
include_directories(${CMAKE_CURRENT_BINARY_DIR})
include_directories(${CMAKE_SOURCE_DIR}/include)

# Configure files
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

# Mac OS X
if(APPLE)
	include_directories(/opt/local/include)
	link_directories(/opt/local/lib)
endif()

# Build CUnit
find_package(CUnit)
if(CUNIT_FOUND)
   add_subdirectory(cunit)
   enable_testing()
endif()

# Sub-directories
add_subdirectory(include)
add_subdirectory(libfreerdp-utils)

if(NOT WIN32)
	add_subdirectory(libfreerdp-kbd)
endif()

add_subdirectory(libfreerdp-gdi)
add_subdirectory(libfreerdp-rail)
add_subdirectory(libfreerdp-cache)
add_subdirectory(libfreerdp-chanman)
add_subdirectory(libfreerdp-core)
add_subdirectory(libfreerdp-rfx)
add_subdirectory(channels)

add_subdirectory(client)
