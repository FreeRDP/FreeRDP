# FreeRDP: A Remote Desktop Protocol Client
# FreeRDP Sample Server cmake build script
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

set(MODULE_NAME "sfreerdp-server")
set(MODULE_PREFIX "FREERDP_SERVER_SAMPLE")

set(${MODULE_PREFIX}_SRCS
	sfreerdp.c
	sfreerdp.h
	sf_audin.c
	sf_audin.h
	sf_rdpsnd.c
	sf_rdpsnd.h)

add_executable(${MODULE_NAME} ${${MODULE_PREFIX}_SRCS})

if(MONOLITHIC_BUILD)
	target_link_libraries(${MODULE_NAME}
		freerdp
		freerdp-channels-server)
else()
	target_link_libraries(${MODULE_NAME}
		freerdp-core
		freerdp-utils
		freerdp-codec
		freerdp-channels
		freerdp-channels-server)
endif()

set_property(TARGET ${MODULE_NAME} PROPERTY FOLDER "Server/Sample")

