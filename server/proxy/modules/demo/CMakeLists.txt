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

cmake_minimum_required(VERSION 3.9)

project(proxy-demo-plugin VERSION 1.0.0 LANGUAGES CXX)

include(CheckIPOSupported)
check_ipo_supported(RESULT supported OUTPUT error)
if (supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(${PROJECT_NAME} MODULE
	demo.cpp
)

target_link_libraries(${PROJECT_NAME} winpr)

set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
set_target_properties(${PROJECT_NAME} PROPERTIES NO_SONAME 1)

install(TARGETS ${PROJECT_NAME} DESTINATION ${FREERDP_PROXY_PLUGINDIR})
