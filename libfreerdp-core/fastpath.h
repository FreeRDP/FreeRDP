/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Fast Path
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

#ifndef __FASTPATH_H
#define __FASTPATH_H

#include "rdp.h"
#include <freerdp/utils/stream.h>

typedef struct rdp_fastpath rdpFastPath;

enum FASTPATH_INPUT_ACTION_TYPE
{
	FASTPATH_INPUT_ACTION_FASTPATH = 0x0,
	FASTPATH_INPUT_ACTION_X224 = 0x3
};

enum FASTPATH_OUTPUT_ACTION_TYPE
{
	FASTPATH_OUTPUT_ACTION_FASTPATH = 0x0,
	FASTPATH_OUTPUT_ACTION_X224 = 0x3
};

enum FASTPATH_INPUT_ENCRYPTION_FLAGS
{
	FASTPATH_INPUT_SECURE_CHECKSUM = 0x1,
	FASTPATH_INPUT_ENCRYPTED = 0x2
};

enum FASTPATH_OUTPUT_ENCRYPTION_FLAGS
{
	FASTPATH_OUTPUT_SECURE_CHECKSUM = 0x1,
	FASTPATH_OUTPUT_ENCRYPTED = 0x2
};

enum FASTPATH_UPDATETYPE
{
	FASTPATH_UPDATETYPE_ORDERS = 0x0,
	FASTPATH_UPDATETYPE_BITMAP = 0x1,
	FASTPATH_UPDATETYPE_PALETTE = 0x2,
	FASTPATH_UPDATETYPE_SYNCHRONIZE = 0x3,
	FASTPATH_UPDATETYPE_SURFCMDS = 0x4,
	FASTPATH_UPDATETYPE_PTR_NULL = 0x5,
	FASTPATH_UPDATETYPE_PTR_DEFAULT = 0x6,
	FASTPATH_UPDATETYPE_PTR_POSITION = 0x8,
	FASTPATH_UPDATETYPE_COLOR = 0x9,
	FASTPATH_UPDATETYPE_CACHED = 0xA,
	FASTPATH_UPDATETYPE_POINTER = 0xB
};

enum FASTPATH_FRAGMENT
{
	FASTPATH_FRAGMENT_SINGLE = 0x0,
	FASTPATH_FRAGMENT_LAST = 0x1,
	FASTPATH_FRAGMENT_FIRST = 0x2,
	FASTPATH_FRAGMENT_NEXT = 0x3
};

enum FASTPATH_OUTPUT_COMPRESSION
{
	FASTPATH_OUTPUT_COMPRESSION_USED = 0x2
};

/* FastPath Input Events */
enum FASTPATH_INPUT_EVENT_CODE
{
	FASTPATH_INPUT_EVENT_SCANCODE = 0x0,
	FASTPATH_INPUT_EVENT_MOUSE = 0x1,
	FASTPATH_INPUT_EVENT_MOUSEX = 0x2,
	FASTPATH_INPUT_EVENT_SYNC = 0x3,
	FASTPATH_INPUT_EVENT_UNICODE = 0x4
};

/* FastPath Keyboard Event Flags */
enum FASTPATH_INPUT_KBDFLAGS
{
	FASTPATH_INPUT_KBDFLAGS_RELEASE = 0x01,
	FASTPATH_INPUT_KBDFLAGS_EXTENDED = 0x02
};

struct rdp_fastpath
{
	rdpRdp* rdp;
	uint8 encryptionFlags;
	uint8 numberEvents;
	STREAM* updateData;
};

uint16 fastpath_header_length(STREAM* s);
uint16 fastpath_read_header(rdpFastPath* fastpath, STREAM* s);
uint16 fastpath_read_header_rdp(rdpFastPath* fastpath, STREAM* s);
boolean fastpath_recv_updates(rdpFastPath* fastpath, STREAM* s);
boolean fastpath_recv_inputs(rdpFastPath* fastpath, STREAM* s);

STREAM* fastpath_input_pdu_init(rdpFastPath* fastpath, uint8 eventFlags, uint8 eventCode);
boolean fastpath_send_input_pdu(rdpFastPath* fastpath, STREAM* s);

STREAM* fastpath_update_pdu_init(rdpFastPath* fastpath);
boolean fastpath_send_update_pdu(rdpFastPath* fastpath, uint8 updateCode, STREAM* s);

boolean fastpath_send_surfcmd_frame_marker(rdpFastPath* fastpath, uint16 frameAction, uint32 frameId);

rdpFastPath* fastpath_new(rdpRdp* rdp);
void fastpath_free(rdpFastPath* fastpath);

#endif
