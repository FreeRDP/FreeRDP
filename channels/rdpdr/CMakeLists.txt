# FreeRDP: A Remote Desktop Protocol Client
# FreeRDP cmake build script
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

set(RDPDR_SRCS
	rdpdr_constants.h
	rdpdr_types.h
	rdpdr_capabilities.c
	rdpdr_capabilities.h
	devman.c
	devman.h
	irp.c
	irp.h
	rdpdr_main.c
	rdpdr_main.h
)

add_library(rdpdr ${RDPDR_SRCS})
set_target_properties(rdpdr PROPERTIES PREFIX "")

target_link_libraries(rdpdr freerdp-utils)

install(TARGETS rdpdr DESTINATION ${FREERDP_PLUGIN_PATH})

add_subdirectory(disk)
add_subdirectory(printer)
add_subdirectory(parallel)
add_subdirectory(serial)

if(WITH_PCSC)
	add_subdirectory(smartcard)
endif()
