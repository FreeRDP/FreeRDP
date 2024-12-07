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

cleaning_configure_file(config/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/winpr/version.h)
cleaning_configure_file(config/build-config.h.in ${CMAKE_CURRENT_BINARY_DIR}/winpr/build-config.h)
cleaning_configure_file(config/buildflags.h.in ${CMAKE_CURRENT_BINARY_DIR}/winpr/buildflags.h)
cleaning_configure_file(config/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/winpr/config.h)

file(GLOB_RECURSE WINPR_PUBLIC_COMMON_HEADERS LIST_DIRECTORIES false "winpr/*.h")

set(WINPR_PUBLIC_TOOLS_HEADERS ${WINPR_PUBLIC_COMMON_HEADERS})
list(FILTER WINPR_PUBLIC_TOOLS_HEADERS INCLUDE REGEX ".*winpr/tools.*")
list(FILTER WINPR_PUBLIC_COMMON_HEADERS EXCLUDE REGEX ".*winpr/tools.*")

file(GLOB_RECURSE WINPR_PUBLIC_COMMON_BIN_HEADERS LIST_DIRECTORIES false "${CMAKE_CURRENT_BINARY_DIR}/*.h")
list(APPEND WINPR_PUBLIC_COMMON_HEADERS ${WINPR_PUBLIC_COMMON_BIN_HEADERS})
list(SORT WINPR_PUBLIC_COMMON_HEADERS)

set_property(TARGET winpr APPEND PROPERTY SOURCES ${WINPR_PUBLIC_COMMON_HEADERS})

if(WITH_WINPR_TOOLS)
  set_property(TARGET winpr-tools APPEND PROPERTY SOURCES ${WINPR_PUBLIC_TOOLS_HEADERS})
endif()

add_library(winpr-headers INTERFACE)
target_sources(winpr-headers INTERFACE ${WINPR_PUBLIC_COMMON_HEADERS})

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/ DESTINATION ${WINPR_INCLUDE_DIR} FILES_MATCHING PATTERN "*.h")

install(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/ DESTINATION ${WINPR_INCLUDE_DIR} FILES_MATCHING PATTERN "*.h")
