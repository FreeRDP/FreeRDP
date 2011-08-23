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

boolean update_recv_surfcmds(rdpUpdate* update, uint16 size, STREAM* s);

int update_write_surfcmd_surface_bits_header(STREAM* s, SURFACE_BITS_COMMAND* cmd);
int update_write_surfcmd_frame_marker(STREAM* s, uint16 frameAction, uint32 frameId);

#endif /* __SURFACE */

