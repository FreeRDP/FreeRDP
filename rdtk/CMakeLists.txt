# RdTk: Remote Desktop Toolkit
# rdtk cmake build script
#
# Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

# Soname versioning
set(RDTK_VERSION_MAJOR "0")
set(RDTK_VERSION_MINOR "2")
set(RDTK_VERSION_REVISION "0")
set(RDTK_VERSION "${RDTK_VERSION_MAJOR}.${RDTK_VERSION_MINOR}.${RDTK_VERSION_REVISION}")
set(RDTK_VERSION_FULL "${RDTK_VERSION}")
set(RDTK_API_VERSION "${RDTK_VERSION_MAJOR}")

if (NOT FREERDP_UNIFIED_BUILD)
	cmake_minimum_required(VERSION 3.4)
	project(RdTk VERSION ${RDTK_VERSION} LANGUAGES C)

	set(CMAKE_COLOR_MAKEFILE ON)

	option(BUILD_SHARED_LIBS "Build shared libraries" ON)
    option(EXPORT_ALL_SYMBOLS "Export all symbols form library" OFF)
	option(BUILD_TESTING "Build library unit tests" ON)
	option(WITH_LIBRARY_VERSIONING "Use library version triplet" ON)

    # Default to release build type
    if(NOT CMAKE_BUILD_TYPE)
       set(CMAKE_BUILD_TYPE "Release")
    endif()

    if(CMAKE_COMPILER_IS_GNUCC)
        if(NOT EXPORT_ALL_SYMBOLS)
            message(STATUS "GCC default symbol visibility: hidden")
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")
        endif()
    endif()
endif()

# Include our extra modules
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/)

# Include cmake modules
include(CheckIncludeFiles)
include(CheckLibraryExists)
include(CheckStructHasMember)
include(FindPkgConfig)
include(TestBigEndian)

# Check for cmake compatibility (enable/disable features)
include(CheckCmakeCompat)
include(FindFeature)
include(CheckCCompilerFlag)
include(GNUInstallDirsWrapper)
include(CMakePackageConfigHelpers)
include (SetFreeRDPCMakeInstallDir)

if(BUILD_SHARED_LIBS)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DRDTK_EXPORTS")
endif()

if (NOT IOS)
	check_include_files(stdbool.h WINPR_HAVE_STDBOOL_H)
	if (NOT WINPR_HAVE_STDBOOL_H)
		include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../compat/stdbool)
	endif()
else()
	set(WINPR_HAVE_STDBOOL_H 1)
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)

if (FREERDP_UNIFIED_BUILD)
	include_directories(${PROJECT_SOURCE_DIR}/winpr/include)
	include_directories(${PROJECT_BINARY_DIR}/winpr/include)
else()
	find_package(WinPR 3 REQUIRED)
	include_directories(${WinPR_INCLUDE_DIR})
endif()

SetFreeRDPCMakeInstallDir(RDTK_CMAKE_INSTALL_DIR "rdtk${RDTK_VERSION_MAJOR}")

include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR}/templates)

add_subdirectory(librdtk)
add_subdirectory(include)
add_subdirectory(templates)

install(EXPORT rdtk DESTINATION ${RDTK_CMAKE_INSTALL_DIR})

if(WITH_SAMPLE)
	if(WITH_X11)
		add_subdirectory(sample)
	endif()
endif()

