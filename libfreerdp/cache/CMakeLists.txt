# FreeRDP: A Remote Desktop Protocol Implementation
# libfreerdp-cache cmake build script
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

set(MODULE_NAME "freerdp-cache")
set(MODULE_PREFIX "FREERDP_CACHE")

freerdp_module_add(
	brush.c
	brush.h
	pointer.c
	pointer.h
	bitmap.c
	bitmap.h
	nine_grid.c
	offscreen.c
	palette.c
	palette.h
	glyph.c
	glyph.h
	cache.c
	cache.h)

