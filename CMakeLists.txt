# FreeRDP: A Remote Desktop Protocol Client
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

cmake_minimum_required(VERSION 2.8)

project(FreeRDP C)

set(CMAKE_COLOR_MAKEFILE ON)

# Include cmake modules
include(CheckIncludeFiles)
include(CheckLibraryExists)
include(CheckStructHasMember)
include(FindPkgConfig)
include(TestBigEndian)

# Include our extra modules
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake/)

include(AutoVersioning)
include(ConfigOptions)
include(BuildFeatureCheck)
include(FindOptionalPackage)
include(CheckCCompilerFlag)
include(GNUInstallDirsWrapper)

# Soname versioning
set(FREERDP_VERSION_MAJOR "1")
set(FREERDP_VERSION_MINOR "1")
set(FREERDP_VERSION_REVISION "0")
set(FREERDP_VERSION "${FREERDP_VERSION_MAJOR}.${FREERDP_VERSION_MINOR}")
set(FREERDP_VERSION_FULL "${FREERDP_VERSION}.${FREERDP_VERSION_REVISION}")
include(GetGitRevisionDescription)
git_describe(GIT_REVISION --match "[0-9]*" --abbrev=4 --tags --always)
message(STATUS "Git Revision ${GIT_REVISION}")

# Turn on solution folders (2.8.4+)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Default to release build type
if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE "Release")
endif()

# Default to build shared libs
if(NOT DEFINED BUILD_SHARED_LIBS)
	set(BUILD_SHARED_LIBS ON)
endif()

if(NOT BUILD_SHARED_LIBS AND MONOLITHIC_BUILD)
	set(STATIC_CHANNELS ON)
endif()

# Configure MSVC Runtime
if(MSVC)
	include(MSVCRuntime)
	if(NOT DEFINED MSVC_RUNTIME)
		set(MSVC_RUNTIME "dynamic")
	endif()
	if(${MSVC_RUNTIME} STREQUAL "static")
		message(STATUS "Use the MSVC static runtime option carefully!")
		message(STATUS "OpenSSL uses /MD by default, and is very picky")
		message(STATUS "Random freeing errors are a common sign of runtime issues")
	endif()
	configure_msvc_runtime()
endif()

# Compiler-specific flags
if(CMAKE_COMPILER_IS_GNUCC)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
	CHECK_C_COMPILER_FLAG (-Wno-unused-result Wno-unused-result)
	if(Wno-unused-result)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-result")
	endif()
	CHECK_C_COMPILER_FLAG (-Wno-unused-but-set-variable Wno-unused-but-set-variable)
	if(Wno-unused-but-set-variable)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-but-set-variable")
	endif()
	if(CMAKE_BUILD_TYPE STREQUAL "Release")
		set(CMAKE_C_FLAGS_RELEASE "-DNDEBUG")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O2")
	endif()
	if(WITH_SSE2_TARGET)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -msse2")
	endif()
endif()

if(MSVC)
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /Gd")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /MT")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /O2")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /Ob2")
	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_AMD64_")
	else()
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_X86_")
	endif()
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_UNICODE")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DWINPR_EXPORTS")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DFREERDP_EXPORTS")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_WIN32_WINNT=0x0501")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_CRT_SECURE_NO_WARNINGS")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DWIN32_LEAN_AND_MEAN")
	SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR})
	SET(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR})
endif()

# config.h definition for installable headers
check_include_files(limits.h FREERDP_HAVE_LIMITS_H)
check_include_files(stdint.h FREERDP_HAVE_STDINT_H)
check_include_files(stdbool.h FREERDP_HAVE_STDBOOL_H)
check_include_files(inttypes.h FREERDP_HAVE_INTTYPES_H)

# Include files
check_include_files(fcntl.h HAVE_FCNTL_H)
check_include_files(unistd.h HAVE_UNISTD_H)
check_include_files(limits.h HAVE_LIMITS_H)
check_include_files(stdint.h HAVE_STDINT_H)
check_include_files(stdbool.h HAVE_STDBOOL_H)
check_include_files(inttypes.h HAVE_INTTYPES_H)
check_include_files(sys/modem.h HAVE_SYS_MODEM_H)
check_include_files(sys/filio.h HAVE_SYS_FILIO_H)
check_include_files(sys/strtio.h HAVE_SYS_STRTIO_H)

check_struct_has_member("struct tm" tm_gmtoff time.h HAVE_TM_GMTOFF)

# Mac OS X
if(APPLE)
	if(IS_DIRECTORY /opt/local/include)
		include_directories(/opt/local/include)
		link_directories(/opt/local/lib)
	endif()
	
	if(WITH_CLANG)
		set(CMAKE_C_COMPILER "clang")
	endif()
	
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mmacosx-version-min=10.4")
	set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl-framework,CoreFoundation")	
endif()

if(NOT WIN32)
	find_required_package(ZLIB)
	find_optional_package(PulseAudio)
	find_optional_package(MacAudio)
	find_optional_package(PCSC)
	find_suggested_package(Cups)

	if(NOT APPLE)
		find_suggested_package(FFmpeg)
		find_suggested_package(XRandR)
		find_suggested_package(Gstreamer)
		find_suggested_package(ALSA)
	else(NOT APPLE)
		find_optional_package(FFmpeg)
	endif()
endif()

if(WIN32)
	set(CMAKE_INSTALL_BINDIR ${CMAKE_INSTALL_PREFIX})
	set(CMAKE_INSTALL_LIBDIR ${CMAKE_INSTALL_PREFIX})
	set(CMAKE_INSTALL_FULL_LIBDIR ${CMAKE_INSTALL_PREFIX})
endif()

# Path to put FreeRDP data
set(FREERDP_DATA_PATH "${CMAKE_INSTALL_PREFIX}/share/freerdp")
set(FREERDP_KEYMAP_PATH "${FREERDP_DATA_PATH}/keymaps")

# Path to put plugins
if(WIN32)
	set(FREERDP_PLUGIN_PATH "${CMAKE_INSTALL_FULL_LIBDIR}")
else()
	set(FREERDP_PLUGIN_PATH "${CMAKE_INSTALL_FULL_LIBDIR}/freerdp")
endif()

set(FREERDP_CLIENT_PLUGIN_PATH "${FREERDP_PLUGIN_PATH}/client")
set(FREERDP_SERVER_PLUGIN_PATH "${FREERDP_PLUGIN_PATH}/server")

# Path to put extensions
set(FREERDP_EXTENSION_PATH "${CMAKE_INSTALL_FULL_LIBDIR}/freerdp/extensions")

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Configure files
add_definitions("-DHAVE_CONFIG_H")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_SOURCE_DIR}/config.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/freerdp/config.h.in ${CMAKE_CURRENT_SOURCE_DIR}/include/freerdp/config.h)

# Unit Tests

INCLUDE(CTest)

if(BUILD_TESTING)
	enable_testing()
endif()

# WinPR
set(WINPR_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/winpr/include")
include_directories(${WINPR_INCLUDE_DIR})

add_subdirectory(winpr)

# Generate pkg-config
if(NOT MSVC)
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/freerdp.pc.in ${CMAKE_CURRENT_BINARY_DIR}/freerdp.pc @ONLY)
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/freerdp.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
endif()

# Intel Performance Primitives
#find_optional_package(IPP)

# Build CUnit

find_optional_package(CUnit)
if(WITH_CUNIT)
	add_subdirectory(cunit)
endif()

# Sub-directories

add_subdirectory(include)

add_subdirectory(libfreerdp)

if(WITH_CHANNELS)
	add_subdirectory(channels)
endif()

if(WITH_CLIENT)
	add_subdirectory(client)
endif()

if(WITH_SERVER)
	add_subdirectory(server)
endif()

if(WITH_THIRD_PARTY)
	add_subdirectory(third-party)
endif()

if(NOT MSVC)
	add_subdirectory(keymaps)
endif()

# Source package
set(CPACK_SOURCE_IGNORE_FILES "/\\\\.git/;/\\\\.gitignore;/CMakeCache.txt")

string(TOLOWER ${CMAKE_PROJECT_NAME} CMAKE_PROJECT_NAME_lower)
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME_lower}-${FREERDP_VERSION_FULL}")

include(CPack)

