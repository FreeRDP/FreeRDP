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

cmake_minimum_required(VERSION 2.6)
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
include(FindOptionalPackage)
include(CheckCCompilerFlag)
include(GNUInstallDirsWrapper)

# Soname versioning
set(FREERDP_VERSION_MAJOR "1")
set(FREERDP_VERSION_MINOR "0")
set(FREERDP_VERSION_REVISION "1")
set(FREERDP_VERSION "${FREERDP_VERSION_MAJOR}.${FREERDP_VERSION_MINOR}")
set(FREERDP_VERSION_FULL "${FREERDP_VERSION}.${FREERDP_VERSION_REVISION}")
include(GetGitRevisionDescription)
git_describe(GIT_REVISION --match "[0-9]*" --abbrev=4 --tags --always)
message(STATUS "Git Revision ${GIT_REVISION}")

# Default to release build type
if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE "Release")
endif()

# Default to build shared libs
if(NOT DEFINED BUILD_SHARED_LIBS)
	set(BUILD_SHARED_LIBS ON)
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
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /Gd /MT")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} /O2 /Ob2")
	if(CMAKE_SIZEOF_VOID_P EQUAL 8)
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_AMD64_")
	else()
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_X86_")
	endif()
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_UNICODE")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DWINPR_EXPORTS")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DFREERDP_EXPORTS")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -D_CRT_SECURE_NO_WARNINGS")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DWIN32_LEAN_AND_MEAN")
	SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR})
	SET(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR})
endif()

# This forces the MSVC runtime to be statically linked

if(MSVC)
	foreach(flag_var CMAKE_C_FLAGS_DEBUG CMAKE_CXX_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE CMAKE_CXX_FLAGS_RELEASE CMAKE_C_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO CMAKE_CXX_FLAGS_RELWITHDEBINFO) 
		string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
		string(REGEX REPLACE "/MDd" "/MTd" ${flag_var} "${${flag_var}}")
	endforeach(flag_var)
	set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}" CACHE STRING "MSVC C Debug MT flags " FORCE)    
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}" CACHE STRING "MSVC CXX Debug MT flags " FORCE)
	set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}" CACHE STRING "MSVC C Release MT flags " FORCE)
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}" CACHE STRING "MSVC CXX Release MT flags " FORCE)
	set(CMAKE_C_FLAGS_MINSIZEREL "${CMAKE_C_FLAGS_MINSIZEREL}" CACHE STRING "MSVC C Debug MT flags " FORCE)    
	set(CMAKE_CXX_FLAGS_MINSIZEREL "${CMAKE_CXX_FLAGS_MINSIZEREL}" CACHE STRING "MSVC C Release MT flags " FORCE)
	set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO}" CACHE STRING "MSVC CXX Debug MT flags " FORCE)    
	set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO}" CACHE STRING "MSVC CXX Release MT flags " FORCE)
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

# Libraries that we have a hard dependency on
if(NOT DEFINED OPENSSL_INCLUDE_DIR OR NOT DEFINED OPENSSL_LIBRARIES)
find_required_package(OpenSSL)
endif()

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

# Path to put FreeRDP data
set(FREERDP_DATA_PATH "${CMAKE_INSTALL_PREFIX}/share/freerdp")
set(FREERDP_KEYMAP_PATH "${FREERDP_DATA_PATH}/keymaps")

# Path to put plugins
set(FREERDP_PLUGIN_PATH "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/freerdp")

# Path to put extensions
set(FREERDP_EXTENSION_PATH "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/freerdp/extensions")

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/include)

# WinPR
set(WINPR_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/winpr/include")
include_directories(${WINPR_INCLUDE_DIR})

# Configure files
add_definitions("-DHAVE_CONFIG_H")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_SOURCE_DIR}/config.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/freerdp/config.h.in ${CMAKE_CURRENT_SOURCE_DIR}/include/freerdp/config.h)

# Generate pkg-config
if(NOT MSVC)
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/freerdp.pc.in ${CMAKE_CURRENT_BINARY_DIR}/freerdp.pc @ONLY)
	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/freerdp.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
endif()

# Build CUnit
find_optional_package(CUnit)
if(WITH_CUNIT)
	enable_testing()
	add_subdirectory(cunit)
endif()

# Sub-directories

add_subdirectory(include)

add_subdirectory(winpr)

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
