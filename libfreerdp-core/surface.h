/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Surface Commands
 *
 * Copyright 2011 Vic Lee
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __SURFACE
#define __SURFACE

#include "rdp.h"
#include <freerdp/utils/stream.h>

#define SURFCMD_SURFACE_BITS_HEADER_LENGTH 22
#define SURFCMD_FRAME_MARKER_LENGTH 8

enum SURFCMD_CMDTYPE
{
	CMDTYPE_SET_SURFACE_BITS = 0x0001,
	CMDTYPE_FRAME_MARKER = 0x0004,
	CMDTYPE_STREAM_SURFACE_BITS = 0x0006
};

enum SURFCMD_FRAMEACTION
{
	SURFACECMD_FRAMEACTION_BEGIN = 0x0000,
	SURFACECMD_FRAMEACTION_END = 0x0001
};

boolean update_recv_surfcmds(rdpUpdate* update, uint32 size, STREAM* s);

void update_write_surfcmd_surface_bits_header(STREAM* s, SURFACE_BITS_COMMAND* cmd);
void update_write_surfcmd_frame_marker(STREAM* s, uint16 frameAction, uint32 frameId);

#endif /* __SURFACE */

