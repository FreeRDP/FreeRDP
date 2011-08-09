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

enum FASTPATH_OUTPUT_ACTION_TYPE
{
	FASTPATH_OUTPUT_ACTION_FASTPATH = 0x0,
	FASTPATH_OUTPUT_ACTION_X224 = 0x3
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

enum SURFCMD_CMDTYPE
{
	CMDTYPE_SET_SURFACE_BITS = 0x0001,
	CMDTYPE_FRAME_MARKER = 0x0004,
	CMDTYPE_STREAM_SURFACE_BITS = 0x0006
};

enum FASTPATH_OUTPUT_COMPRESSION
{
	FASTPATH_OUTPUT_COMPRESSION_USED = 0x2
};

struct rdp_fastpath
{
	rdpRdp* rdp;
	uint8 encryptionFlags;
	STREAM* updateData;
};

uint16 fastpath_read_header(rdpFastPath* fastpath, STREAM* s);
void fastpath_recv_updates(rdpFastPath* fastpath, STREAM* s);

rdpFastPath* fastpath_new(rdpRdp* rdp);
void fastpath_free(rdpFastPath* fastpath);

#endif
