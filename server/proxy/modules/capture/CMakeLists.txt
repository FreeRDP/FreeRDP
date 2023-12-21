#
# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP Proxy Server Capture Module
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

if (NOT FREERDP_DEFAULT_PROJECT_VERSION)
	set(FREERDP_DEFAULT_PROJECT_VERSION "1.0.0.0")
endif()

project(proxy-capture-plugin
	VERSION ${FREERDP_DEFAULT_PROJECT_VERSION}
       	LANGUAGES C
)

message("project ${PROJECT_NAME} is using version ${PROJECT_VERSION}")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

include(CheckIPOSupported)
check_ipo_supported(RESULT supported OUTPUT error)
if (supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
endif()

include(MSVCRuntime)

add_library(${PROJECT_NAME} SHARED
	cap_main.c
	cap_config.c
	cap_config.h
	cap_protocol.c
	cap_protocol.h
)

target_link_libraries(${PROJECT_NAME} winpr freerdp)

set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
set_target_properties(${PROJECT_NAME} PROPERTIES NO_SONAME 1)

install(TARGETS ${PROJECT_NAME} DESTINATION ${FREERDP_PROXY_PLUGINDIR})
