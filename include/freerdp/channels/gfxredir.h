/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDPXXXX Remote App Graphics Redirection Virtual Channel Extension
 *
 * Copyright 2020 Hideyuki Nagase <hideyukn@microsoft.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	 http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef FREERDP_CHANNEL_GFXREDIR_H
#define FREERDP_CHANNEL_GFXREDIR_H

#include <freerdp/api.h>
#include <freerdp/types.h>

#define GFXREDIR_DVC_CHANNEL_NAME "Microsoft::Windows::RDS::RemoteAppGraphicsRedirection"

/* GFXREDIR_LEGACY_CAPS_PDU.version */
#define GFXREDIR_CHANNEL_VERSION_LEGACY 1

#define GFXREDIR_CHANNEL_VERSION_MAJOR 2
#define GFXREDIR_CHANNEL_VERSION_MINOR 0

/* GFXREDIR_CAPS_VERSION1 */
#define GFXREDIR_CMDID_LEGACY_CAPS 0x00000001
#define GFXREDIR_CMDID_ERROR 0x00000006
#define GFXREDIR_CMDID_CAPS_ADVERTISE 0x00000008
#define GFXREDIR_CMDID_CAPS_CONFIRM 0x00000009
/* GFXREDIR_CAPS_VERSION2_0 */
#define GFXREDIR_CMDID_OPEN_POOL 0x0000000a
#define GFXREDIR_CMDID_CLOSE_POOL 0x0000000b
#define GFXREDIR_CMDID_CREATE_BUFFER 0x0000000c
#define GFXREDIR_CMDID_DESTROY_BUFFER 0x0000000d
#define GFXREDIR_CMDID_PRESENT_BUFFER 0x0000000e
#define GFXREDIR_CMDID_PRESENT_BUFFER_ACK 0x0000000f

/* GFXREDIR_HEADER */
#define GFXREDIR_HEADER_SIZE 8

/* GFXREDIR_CAPS_HEADER */
#define GFXREDIR_CAPS_HEADER_SIZE 12
/* GFXREDIR_CAPS_HEADER.signature */
#define GFXREDIR_CAPS_SIGNATURE 0x53504143 /* = 'SPAC' */
/* GFXREDIR_CAPS_HEADER.version */
#define GFXREDIR_CAPS_VERSION1 0x1
#define GFXREDIR_CAPS_VERSION2_0 0x2000

/* GFXREDIR_CREATE_BUFFER_PDU.format */
#define GFXREDIR_BUFFER_PIXEL_FORMAT_XRGB_8888 1
#define GFXREDIR_BUFFER_PIXEL_FORMAT_ARGB_8888 2

/* GFXREDIR_PRESENT_BUFFER_PDU.numOpaqueRects */
#define GFXREDIR_MAX_OPAQUE_RECTS 0x10

typedef struct
{
	UINT32 cmdId;
	UINT32 length;
} GFXREDIR_HEADER;

typedef struct
{
	UINT16 version; // GFXREDIR_CHANNEL_VERSION_LEGACY
} GFXREDIR_LEGACY_CAPS_PDU;

typedef struct
{
	UINT32 signature; // GFXREDIR_CAPS_SIGNATURE
	UINT32 version;   // GFXREDIR_CAPS_VERSION
	UINT32 length;    // GFXREDIR_CAPS_HEADER_SIZE + size of capsData
} GFXREDIR_CAPS_HEADER;

typedef struct
{
	GFXREDIR_CAPS_HEADER header;
	UINT32 supportedFeatures; /* Reserved for future extensions */
} GFXREDIR_CAPS_V2_0_PDU;

typedef struct
{
	UINT32 errorCode;
} GFXREDIR_ERROR_PDU;

typedef struct
{
	UINT32 length;    // length of caps;
	const BYTE* caps; // points variable length array of GFXREDIR_CAPS_HEADER.
} GFXREDIR_CAPS_ADVERTISE_PDU;

typedef struct
{
	UINT32 version;       // confirmed version, must be one of advertised by client.
	UINT32 length;        // GFXREDIR_CAPS_HEADER_SIZE + size of capsData.
	const BYTE* capsData; // confirmed capsData from selected GFXREDIR_CAPS_HEADER.capsData.
} GFXREDIR_CAPS_CONFIRM_PDU;

typedef struct
{
	UINT64 poolId;
	UINT64 poolSize;
	UINT32 sectionNameLength;          // number of charactor, must include null terminated char.
	const unsigned short* sectionName; // Windows-style 2 bytes wchar_t with null-terminated.
} GFXREDIR_OPEN_POOL_PDU;

typedef struct
{
	UINT64 poolId;
} GFXREDIR_CLOSE_POOL_PDU;

typedef struct
{
	UINT64 poolId;
	UINT64 bufferId;
	UINT64 offset;
	UINT32 stride;
	UINT32 width;
	UINT32 height;
	UINT32 format; // GFXREDIR_BUFFER_PIXEL_FORMAT_
} GFXREDIR_CREATE_BUFFER_PDU;

typedef struct
{
	UINT64 bufferId;
} GFXREDIR_DESTROY_BUFFER_PDU;

typedef struct
{
	UINT64 timestamp;
	UINT64 presentId;
	UINT64 windowId;
	UINT64 bufferId;
	UINT32 orientation; // 0, 90, 180 or 270.
	UINT32 targetWidth;
	UINT32 targetHeight;
	RECTANGLE_32 dirtyRect;
	UINT32 numOpaqueRects;
	RECTANGLE_32* opaqueRects;
} GFXREDIR_PRESENT_BUFFER_PDU;

typedef struct
{
	UINT64 windowId;
	UINT64 presentId;
} GFXREDIR_PRESENT_BUFFER_ACK_PDU;

#endif /* FREERDP_CHANNEL_GFXREDIR_H */
