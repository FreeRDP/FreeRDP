# FreeRDP: A Remote Desktop Protocol Client
# FreeRDP cmake build script
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

set(DRDYNVC_SRCS
	drdynvc_main.c
	drdynvc_main.h
	drdynvc_types.h
	dvcman.c
	dvcman.h
)

add_library(drdynvc ${DRDYNVC_SRCS})
set_target_properties(drdynvc PROPERTIES PREFIX "")

target_link_libraries(drdynvc freerdp-utils)

install(TARGETS drdynvc DESTINATION ${FREERDP_PLUGIN_PATH})

add_subdirectory(tsmf)
add_subdirectory(audin)
add_subdirectory(urbdrc)

if(WITH_CLIENT_CHANNELS)
	add_subdirectory(client)
endif()


