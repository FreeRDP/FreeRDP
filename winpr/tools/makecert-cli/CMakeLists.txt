# WinPR: Windows Portable Runtime
# winpr-makecert cmake build script
#
# Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
# Copyright 2016 Thincast Technologies GmbH
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

set(MODULE_NAME "winpr-makecert")
set(MODULE_PREFIX "WINPR_MAKECERT")

set(${MODULE_PREFIX}_SRCS main.c)

addtargetwithresourcefile(${MODULE_NAME} TRUE "${WINPR_VERSION}" ${MODULE_PREFIX}_SRCS)

set(${MODULE_PREFIX}_LIBS winpr-tools)

target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS} winpr)

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "WinPR/Tools")

install(TARGETS ${MODULE_NAME} DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT tools EXPORT WinPRTargets)

generate_and_install_freerdp_man_from_template(${MODULE_NAME} "1" "${WINPR_API_VERSION}")
