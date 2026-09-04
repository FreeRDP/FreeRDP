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

set(MODULE_NAME "freerdp-webview-aad-helper")

# the top-level project only declares C; this is the first CXX target reached in the build
# (client/SDL is the other one, but it's processed later and enables CXX itself)
enable_language(CXX)

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

add_executable(${MODULE_NAME} main.cpp redirect_watcher.hpp redirect_watcher.cpp)
set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Client/Common")

target_link_libraries(${MODULE_NAME} PRIVATE winpr webview::core)

installwithrpath(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT client)
