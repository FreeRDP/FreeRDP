# FreeRDP: A Remote Desktop Protocol Client
# keymaps cmake build script
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

if(NOT WITH_XKBFILE)
	install(FILES aliases amiga ataritt empty evdev fujitsu hp ibm macintosh macosx sony sun xfree86 xfree98 DESTINATION ${FREERDP_KEYMAP_PATH})
	install(DIRECTORY digital_vndr DESTINATION ${FREERDP_KEYMAP_PATH} FILES_MATCHING PATTERN "*")
	install(DIRECTORY sgi_vndr DESTINATION ${FREERDP_KEYMAP_PATH} FILES_MATCHING PATTERN "*")
endif()
