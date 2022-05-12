/**
 * FreeRDP: A Remote Desktop Protocol Implementation
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

#ifndef FREERDP_LIB_CORE_FASTPATH_H
#define FREERDP_LIB_CORE_FASTPATH_H

/*
 * Fast-Path has 15 bits available for length information which would lead to a
 * maximal pdu size of 0x8000. However in practice only 14 bits are used
 * this isn't documented anywhere but it looks like most implementations will
 * fail if fast-path packages > 0x3FFF arrive.
 */
#define FASTPATH_MAX_PACKET_SIZE 0x3FFF

/*
 *  The following size guarantees that no fast-path PDU fragmentation occurs.
 *  It was calculated by subtracting 128 from FASTPATH_MAX_PACKET_SIZE.
 *  128 was chosen because it includes all required and optional headers as well as
 *  possible paddings and some extra bytes for safety.
 */
#define FASTPATH_FRAGMENT_SAFE_SIZE 0x3F80

typedef struct rdp_fastpath rdpFastPath;

#include "rdp.h"

#include <winpr/stream.h>
#include <freerdp/api.h>

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
	FASTPATH_UPDATETYPE_POINTER = 0xB,
	FASTPATH_UPDATETYPE_LARGE_POINTER = 0xC
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
	FASTPATH_INPUT_KBDFLAGS_EXTENDED = 0x02,
	FASTPATH_INPUT_KBDFLAGS_PREFIX_E1 = 0x04 /* for pause sequence */
};

typedef struct
{
	BYTE fpOutputHeader;
	BYTE length1;
	BYTE length2;
	BYTE fipsInformation[4];
	BYTE dataSignature[8];

	BYTE action;
	BYTE secFlags;
	UINT16 length;
} FASTPATH_UPDATE_PDU_HEADER;

typedef struct
{
	BYTE updateHeader;
	BYTE compressionFlags;
	UINT16 size;

	BYTE updateCode;
	BYTE fragmentation;
	BYTE compression;
} FASTPATH_UPDATE_HEADER;

FREERDP_LOCAL BOOL fastpath_read_header_rdp(rdpFastPath* fastpath, wStream* s, UINT16* length);
FREERDP_LOCAL int fastpath_recv_updates(rdpFastPath* fastpath, wStream* s);
FREERDP_LOCAL int fastpath_recv_inputs(rdpFastPath* fastpath, wStream* s);

FREERDP_LOCAL wStream* fastpath_input_pdu_init_header(rdpFastPath* fastpath);
FREERDP_LOCAL wStream* fastpath_input_pdu_init(rdpFastPath* fastpath, BYTE eventFlags,
                                               BYTE eventCode);
FREERDP_LOCAL BOOL fastpath_send_multiple_input_pdu(rdpFastPath* fastpath, wStream* s,
                                                    size_t iEventCount);
FREERDP_LOCAL BOOL fastpath_send_input_pdu(rdpFastPath* fastpath, wStream* s);

FREERDP_LOCAL wStream* fastpath_update_pdu_init(rdpFastPath* fastpath);
FREERDP_LOCAL wStream* fastpath_update_pdu_init_new(rdpFastPath* fastpath);
FREERDP_LOCAL BOOL fastpath_send_update_pdu(rdpFastPath* fastpath, BYTE updateCode, wStream* s,
                                            BOOL skipCompression);

FREERDP_LOCAL BOOL fastpath_send_surfcmd_frame_marker(rdpFastPath* fastpath, UINT16 frameAction,
                                                      UINT32 frameId);
FREERDP_LOCAL BYTE fastpath_get_encryption_flags(rdpFastPath* fastpath);

FREERDP_LOCAL rdpFastPath* fastpath_new(rdpRdp* rdp);
FREERDP_LOCAL void fastpath_free(rdpFastPath* fastpath);

#endif /* FREERDP_LIB_CORE_FASTPATH_H */
