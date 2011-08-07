# FreeRDP: A Remote Desktop Protocol Client
# FreeRDP X11 Client
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

include_directories(../../libfreerdp-gdi)
include_directories(../../libfreerdp-kbd)
include_directories(../../libfreerdp-core)
include_directories(../../libfreerdp-cache)
include_directories(${X11_INCLUDE_DIRS})

add_executable(xfreerdp
	xf_event.c
	xf_event.h
	xf_keyboard.c
	xf_keyboard.h
	xfreerdp.c
	xfreerdp.h)

target_link_libraries(xfreerdp freerdp-core)
target_link_libraries(xfreerdp freerdp-gdi)
target_link_libraries(xfreerdp freerdp-kbd)
target_link_libraries(xfreerdp freerdp-chanman)
target_link_libraries(xfreerdp freerdp-utils)
target_link_libraries(xfreerdp ${X11_LIBRARIES})
