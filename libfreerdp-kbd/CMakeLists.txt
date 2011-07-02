# FreeRDP: A Remote Desktop Protocol Client
# libfreerdp-kbd cmake build script
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

include_directories(.)
include_directories(../include)

set(FREERDP_KBD_SRCS
	keyboard.h
	locales.c
	locales.h
	layout_ids.c
	layout_ids.h
	layouts_xkb.c
	layouts_xkb.h
	x_layout_id_table.c
	x_layout_id_table.h
	libkbd.c
	libkbd.h)

add_definitions(-DKEYMAP_PATH="${FREERDP_KEYMAP_PATH}")

add_library(freerdp-kbd SHARED ${FREERDP_KBD_SRCS})

set_target_properties(freerdp-kbd PROPERTIES VERSION ${FREERDP_VERSION_FULL} SOVERSION ${FREERDP_VERSION})

install(TARGETS freerdp-kbd DESTINATION lib)

