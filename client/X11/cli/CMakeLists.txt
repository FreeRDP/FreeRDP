# FreeRDP: A Remote Desktop Protocol Implementation
# FreeRDP X11 cmake build script
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

set(MODULE_NAME "xfreerdp-cli")
set(MODULE_PREFIX "FREERDP_CLIENT_X11")

set(${MODULE_PREFIX}_SRCS
	xfreerdp.c)

add_executable(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})
set_target_properties(${MODULE_NAME} PROPERTIES OUTPUT_NAME "xfreerdp" RUNTIME_OUTPUT_DIRECTORY "..")

set(${MODULE_PREFIX}_LIBS ${${MODULE_PREFIX}_LIBS} xfreerdp-client freerdp-client)

if(OPENBSD)
	target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS} ossaudio)
else()
	target_link_libraries(${MODULE_NAME} ${${MODULE_PREFIX}_LIBS})
endif()

install(TARGETS ${MODULE_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT client)

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Client/X11")

