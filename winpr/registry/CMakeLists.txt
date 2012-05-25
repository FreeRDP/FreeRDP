# WinPR: Windows Portable Runtime
# libwinpr-registry cmake build script
#
# Copyright 2011 O.S. Systems Software Ltda.
# Copyright 2011 Otavio Salvador <otavio@ossystems.com.br>
# Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

if(NOT WIN32)
	set(WITH_REGISTRY_XML true)
endif()

set(WINPR_REGISTRY_SRCS
	registry.c)

set(WINPR_REGISTRY_XML_SRCS
	registry_xml.c
	registry_xml.h)

if(WITH_REGISTRY_XML)
	set(WINPR_REGISTRY_SRCS ${WINPR_REGISTRY_SRCS} ${WINPR_REGISTRY_XML_SRCS})
endif()

add_library(winpr-registry ${WINPR_REGISTRY_SRCS})

set_target_properties(winpr-registry PROPERTIES VERSION ${FREERDP_VERSION_FULL} SOVERSION ${FREERDP_VERSION} PREFIX "lib")

if(WITH_REGISTRY_XML)
	find_required_package(LibXml2)
	add_definitions("${LIBXML2_DEFINITIONS}")
	include_directories(SYSTEM ${LIBXML2_INCLUDE_DIR})
	target_link_libraries(winpr-registry ${LIBXML2_LIBRARIES})
endif()

target_link_libraries(winpr-registry winpr-utils)

install(TARGETS winpr-registry DESTINATION ${CMAKE_INSTALL_LIBDIR})

