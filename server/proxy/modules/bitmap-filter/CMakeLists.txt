#
# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP Proxy Server Demo C++ Module
#
# Copyright 2019 Kobi Mizrachi <kmizrachi18@gmail.com>
# Copyright 2021 Armin Novak <anovak@thincast.com>
# Copyright 2021 Thincast Technologies GmbH
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
#

cmake_minimum_required(VERSION 3.13)

if(POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW)
endif()
if(NOT FREERDP_DEFAULT_PROJECT_VERSION)
  set(FREERDP_DEFAULT_PROJECT_VERSION "1.0.0.0")
endif()

project(proxy-bitmap-filter-plugin VERSION ${FREERDP_DEFAULT_PROJECT_VERSION} LANGUAGES CXX)

message("project ${PROJECT_NAME} is using version ${PROJECT_VERSION}")

set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../../../../cmake/)
include(CommonConfigOptions)
include(CXXCompilerFlags)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SRCS bitmap-filter.cpp)
addtargetwithresourcefile(${PROJECT_NAME} FALSE "${PROJECT_VERSION}" SRCS FALSE)

target_link_libraries(${PROJECT_NAME} winpr freerdp)

install(TARGETS ${PROJECT_NAME} DESTINATION ${FREERDP_PROXY_PLUGINDIR})
