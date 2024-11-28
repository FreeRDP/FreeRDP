# UWAC: Using Wayland As Client
# cmake build script
#
# Copyright 2015 David FORT <contact@hardening-consulting.com>
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
set(UWAC_VERSION_MAJOR "0")
set(UWAC_VERSION_MINOR "2")
set(UWAC_VERSION_REVISION "0")
set(UWAC_VERSION "${UWAC_VERSION_MAJOR}.${UWAC_VERSION_MINOR}.${UWAC_VERSION_REVISION}")
set(UWAC_VERSION_FULL "${UWAC_VERSION}")
set(UWAC_API_VERSION "${UWAC_VERSION_MAJOR}")

if(NOT FREERDP_UNIFIED_BUILD)
  cmake_minimum_required(VERSION 3.13)
  project(uwac VERSION ${UWAC_VERSION} LANGUAGES C)

  set(CMAKE_C_STANDARD 11)
  set(CMAKE_C_STANDARD_REQUIRED ON)
  set(CMAKE_C_EXTENSIONS ON)

  option(EXPORT_ALL_SYMBOLS "Export all symbols form library" OFF)
  option(BUILD_TESTING_INTERNAL "Build library unit tests" ON)

  if(CMAKE_COMPILER_IS_GNUCC)
    if(NOT EXPORT_ALL_SYMBOLS)
      message(STATUS "GCC default symbol visibility: hidden")
      set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fvisibility=hidden")
    endif()
  endif()
endif()

option(UWAC_FORCE_STATIC_BUILD "Force UWAC to be built as static library (recommended)" OFF)
option(UWAC_HAVE_PIXMAN_REGION "Use PIXMAN or FreeRDP for region calculations" "NOT FREERDP_UNIFIED_BUILD")

# Include our extra modules
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../cmake/)
include(CommonConfigOptions)

include(CheckFunctionExists)
check_function_exists(strerror_r UWAC_HAVE_STRERROR_R)

# Check for cmake compatibility (enable/disable features)
include(FindFeature)

if(UWAC_FORCE_STATIC_BUILD)
  set(BUILD_SHARED_LIBS OFF)
else()
  include(SetFreeRDPCMakeInstallDir)
  include(CMakePackageConfigHelpers)
endif()

if(NOT IOS)
  include(CheckIncludeFiles)
  check_include_files(stdbool.h UWAC_HAVE_STDBOOL_H)
  if(NOT UWAC_HAVE_STDBOOL_H)
    include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/../compat/stdbool)
  endif()
endif()

# Find required libraries
if(UWAC_HAVE_PIXMAN_REGION)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(pixman REQUIRED pixman-1)
  include_directories(SYSTEM ${pixman_INCLUDE_DIRS})
elseif(FREERDP_UNIFIED_BUILD)
  include_directories(${PROJECT_SOURCE_DIR}/winpr/include)
  include_directories(${PROJECT_BINARY_DIR}/winpr/include)
  include_directories(${PROJECT_SOURCE_DIR}/include)
  include_directories(${PROJECT_BINARY_DIR}/include)
else()
  find_package(WinPR 3 REQUIRED)
  find_package(FreeRDP 3 REQUIRED)
  include_directories(SYSTEM ${WinPR_INCLUDE_DIR})
  include_directories(SYSTEM ${FreeRDP_INCLUDE_DIR})
endif()

set(WAYLAND_FEATURE_PURPOSE "Wayland")
set(WAYLAND_FEATURE_DESCRIPTION "Wayland client")
set(WAYLAND_FEATURE_TYPE "REQUIRED")
find_feature(Wayland ${WAYLAND_FEATURE_TYPE} ${WAYLAND_FEATURE_PURPOSE} ${WAYLAND_FEATURE_DESCRIPTION})

set(UWAC_INCLUDE_DIR include/uwac${UWAC_API_VERSION})

include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/include)
include_directories(BEFORE ${CMAKE_CURRENT_BINARY_DIR}/include)

add_subdirectory(libuwac)
add_subdirectory(templates)
add_subdirectory(include)
