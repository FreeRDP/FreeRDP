# FreeRDP: A Remote Desktop Protocol Implementation
# libfreerdp-locale cmake build script
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

set(MODULE_NAME "freerdp-locale")
set(MODULE_PREFIX "FREERDP_LOCALE")

set(${MODULE_PREFIX}_SRCS
	keyboard_layout.c
	keyboard.c
	locale.c
	liblocale.h)

set(${MODULE_PREFIX}_X11_SRCS
	keyboard_x11.c
	keyboard_x11.h
	xkb_layout_ids.c
	xkb_layout_ids.h)

set(${MODULE_PREFIX}_XKBFILE_SRCS
	keyboard_xkbfile.c
	keyboard_xkbfile.h)

set(${MODULE_PREFIX}_SUN_SRCS
	keyboard_sun.c
	keyboard_sun.h)

if(CMAKE_SYSTEM_NAME MATCHES Solaris)
	set(WITH_SUN true)
endif()

if(WITH_X11)
	freerdp_definition_add(-DWITH_X11)
	freerdp_include_directory_add(${X11_INCLUDE_DIRS})
	set(${MODULE_PREFIX}_SRCS ${${MODULE_PREFIX}_SRCS} ${${MODULE_PREFIX}_X11_SRCS})
	freerdp_library_add(${X11_LIBRARIES})

	if(WITH_SUN)
		freerdp_definition_add(-DWITH_SUN)
		set(${MODULE_PREFIX}_SRCS ${${MODULE_PREFIX}_SRCS} ${${MODULE_PREFIX}_SUN_SRCS})
	endif()

	set(XKBFILE_FEATURE_TYPE "RECOMMENDED")
	set(XKBFILE_FEATURE_PURPOSE "X11 keyboard")
	set(XKBFILE_FEATURE_DESCRIPTION "X11 keyboard file extension")

	find_feature(XKBFile ${XKBFILE_FEATURE_TYPE} ${XKBFILE_FEATURE_PURPOSE} ${XKBFILE_FEATURE_DESCRIPTION})

	if(WITH_XKBFILE AND (NOT APPLE))
		freerdp_definition_add(-DWITH_XKBFILE)
		freerdp_include_directory_add(${XKBFILE_INCLUDE_DIRS})
		set(${MODULE_PREFIX}_SRCS ${${MODULE_PREFIX}_SRCS} ${${MODULE_PREFIX}_XKBFILE_SRCS})
		freerdp_library_add(${XKBFILE_LIBRARIES})
	else()
		set(${MODULE_PREFIX}_SRCS ${${MODULE_PREFIX}_SRCS} ${${MODULE_PREFIX}_X11_KEYMAP_SRCS})
	endif()
endif()

freerdp_module_add(${${MODULE_PREFIX}_SRCS})
