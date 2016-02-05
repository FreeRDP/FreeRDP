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

cmake_minimum_required(VERSION 2.8)

project(RdTk C)

set(CMAKE_COLOR_MAKEFILE ON)

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
include(CMakePackageConfigHelpers)

# Soname versioning
set(RDTK_VERSION_MAJOR "0")
set(RDTK_VERSION_MINOR "0")
set(RDTK_VERSION_REVISION "1")
set(RDTK_API_VERSION "${RDTK_VERSION_MAJOR}")
set(RDTK_VERSION "${RDTK_VERSION_MAJOR}.${RDTK_VERSION_MINOR}.${RDTK_VERSION_REVISION}")
set(RDTK_VERSION_FULL "${RDTK_VERSION}")

# Default to release build type
if(NOT CMAKE_BUILD_TYPE)
   set(CMAKE_BUILD_TYPE "Release")
endif()

# Default to build shared libs
if(NOT DEFINED BUILD_SHARED_LIBS)
	set(BUILD_SHARED_LIBS ON)
endif()

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DRDTK_EXPORTS")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)

add_subdirectory(include)
add_subdirectory(librdtk)

if(WITH_SAMPLE)
	if(WITH_X11)
		add_subdirectory(sample)
	endif()
endif()

# Exporting
set(RDTK_INCLUDE_DIR "include/rdtk${RDTK_VERSION_MAJOR}")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/rdtk.pc.in ${CMAKE_CURRENT_BINARY_DIR}/rdtk${RDTK_VERSION_MAJOR}.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/rdtk${RDTK_VERSION_MAJOR}.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

if(${CMAKE_VERSION} VERSION_GREATER "2.8.10")
	export(PACKAGE rdtk)

	set(RDTK_CMAKE_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/RdTk${RDTK_VERSION_MAJOR}")

	configure_package_config_file(RdTkConfig.cmake.in ${CMAKE_CURRENT_BINARY_DIR}/RdTkConfig.cmake
		INSTALL_DESTINATION ${RDTK_CMAKE_INSTALL_DIR} PATH_VARS RDTK_INCLUDE_DIR)

	write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/RdTkConfigVersion.cmake
		VERSION ${RDTK_VERSION} COMPATIBILITY SameMajorVersion)

	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/RdTkConfig.cmake ${CMAKE_CURRENT_BINARY_DIR}/RdTkConfigVersion.cmake
		DESTINATION ${RDTK_CMAKE_INSTALL_DIR})

	install(EXPORT RdTkTargets DESTINATION ${RDTK_CMAKE_INSTALL_DIR})
endif()

