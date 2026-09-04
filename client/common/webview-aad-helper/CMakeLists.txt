# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP out-of-process AAD auth helper
#
# Copyright 2024 Armin Novak <anovak@thincast.com>
# Copyright 2024 Thincast Technologies GmbH
# Copyright 2026 David Fort <contact@hardening-consulting.com>
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

cmake_minimum_required(VERSION 3.13)
if(POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW)
endif()
if(NOT FREERDP_DEFAULT_PROJECT_VERSION)
  set(FREERDP_DEFAULT_PROJECT_VERSION "1.0.0.0")
endif()

project(freerdp-webview-aad-helper VERSION ${FREERDP_DEFAULT_PROJECT_VERSION} LANGUAGES CXX)

include(FetchContent)

set(FETCHCONTENT_SOURCE_DIR_WEBVIEW "${CMAKE_CURRENT_SOURCE_DIR}/../../../external/webview")
if(IS_DIRECTORY "${FETCHCONTENT_SOURCE_DIR_WEBVIEW}")
  message("Using existing source from ${FETCHCONTENT_SOURCE_DIR_WEBVIEW}")
else()
  unset(FETCHCONTENT_SOURCE_DIR_WEBVIEW)
endif()

set(WEBVIEW_BUILD_DOCS OFF CACHE INTERNAL "fetchcontent default")
set(WEBVIEW_BUILD_SHARED_LIBRARY OFF CACHE INTERNAL "fetchcontent default")
set(WEBVIEW_BUILD_STATIC_LIBRARY ON CACHE INTERNAL "fetchcontent default")
set(WEBVIEW_BUILD_TESTS OFF CACHE INTERNAL "fetchcontent default")
set(WEBVIEW_BUILD_EXAMPLES OFF CACHE INTERNAL "fetchcontent default")
FetchContent_Declare(webview GIT_REPOSITORY https://github.com/akallabeth/webview GIT_TAG navigation-listener SYSTEM)
FetchContent_MakeAvailable(webview)

add_executable(${PROJECT_NAME} main.cpp redirect_watcher.hpp redirect_watcher.cpp)
set_property(TARGET ${PROJECT_NAME} PROPERTY FOLDER "Client/Common")

target_link_libraries(${PROJECT_NAME} PRIVATE winpr webview::core)

installwithrpath(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT client)
