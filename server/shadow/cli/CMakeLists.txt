# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP Shadow Server cmake build script
#
# Copyright 2025 Armin Novak <anoavk@thincast.com>
# Copyright Thincast Technologies GmbH
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#	 http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set(MODULE_NAME "freerdp-shadow-cli")

set(SRCS shadow.c)

addtargetwithresourcefile(${MODULE_NAME} TRUE "${FREERDP_VERSION}" SRCS)

list(APPEND LIBS freerdp-shadow-subsystem freerdp-shadow freerdp winpr)

target_link_libraries(${MODULE_NAME} PRIVATE ${LIBS})

installwithrpath(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT server)
generate_and_install_freerdp_man_from_template(${MODULE_NAME} "1" "${FREERDP_API_VERSION}")
