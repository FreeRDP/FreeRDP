# FreeRDP: A Remote Desktop Protocol Client
# FreeRDP DirectFB Client
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

include_directories(${DIRECTFB_INCLUDE_DIRS})

add_executable(dfreerdp
	df_event.c
	df_event.h
	df_graphics.c
	df_graphics.c
	dfreerdp.c
	dfreerdp.h)

target_link_libraries(dfreerdp freerdp-core)
target_link_libraries(dfreerdp freerdp-gdi)
target_link_libraries(dfreerdp freerdp-kbd)
target_link_libraries(dfreerdp freerdp-channels)
target_link_libraries(dfreerdp freerdp-utils)
target_link_libraries(dfreerdp ${DIRECTFB_LIBRARIES})

install(TARGETS dfreerdp DESTINATION ${CMAKE_INSTALL_BINDIR})
