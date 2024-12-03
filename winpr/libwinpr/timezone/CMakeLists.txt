# WinPR: Windows Portable Runtime
# libwinpr-timezone cmake build script
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

option(WITH_TIMEZONE_COMPILED "Use compiled in timezone definitions" ON)
if(WITH_TIMEZONE_COMPILED)
  winpr_definition_add(WITH_TIMEZONE_COMPILED)
endif()

include(CMakeDependentOption)
cmake_dependent_option(WITH_TIMEZONE_FROM_FILE "Use timezone definitions from JSON file" OFF WITH_WINPR_JSON OFF)
if(WITH_TIMEZONE_FROM_FILE)

  winpr_definition_add(WINPR_RESOURCE_ROOT="${WINPR_RESOURCE_ROOT}")
  winpr_definition_add(WITH_TIMEZONE_FROM_FILE)

  install(FILES TimeZoneNameMap.json DESTINATION ${WINPR_RESOURCE_ROOT})
endif()

set(SRCS TimeZoneNameMapUtils.c TimeZoneNameMap.h timezone.c)
if(WITH_TIMEZONE_COMPILED)
  list(APPEND SRCS TimeZoneNameMap_static.h)
endif()

if(NOT WIN32)
  list(APPEND SRCS TimeZoneIanaAbbrevMap.c TimeZoneIanaAbbrevMap.h)
endif()

option(WITH_TIMEZONE_ICU "Use ICU for improved timezone mapping" OFF)
if(WITH_TIMEZONE_ICU)
  find_package(ICU COMPONENTS i18n uc REQUIRED)
  winpr_include_directory_add(${ICU_INCLUDE_DIRS})
  winpr_library_add_private(${ICU_LIBRARIES})
  winpr_definition_add(WITH_TIMEZONE_ICU)
else()
  list(APPEND SRCS WindowsZones.c WindowsZones.h)
endif()

winpr_module_add(${SRCS})

if(WIN32)
  option(WITH_TIMEZONE_UPDATER "Build C# tzextract" OFF)
  if(WITH_TIMEZONE_UPDATER)
    add_subdirectory(utils)
  endif()
endif()
