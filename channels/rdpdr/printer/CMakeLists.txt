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

set(PRINTER_SRCS
	printer_main.c
	printer_main.h
)

if(WITH_CUPS)
	set(PRINTER_SRCS
		${PRINTER_SRCS}
		printer_cups.c
		printer_cups.h
	)
	include_directories(${CUPS_INCLUDE_DIR})
	add_definitions(-DWITH_CUPS)
endif()

include_directories(..)

add_library(printer ${PRINTER_SRCS})
set_target_properties(printer PROPERTIES PREFIX "")

target_link_libraries(printer freerdp-utils)

if(WITH_CUPS)
	target_link_libraries(printer ${CUPS_LIBRARIES})
endif()

install(TARGETS printer DESTINATION ${FREERDP_PLUGIN_PATH})
