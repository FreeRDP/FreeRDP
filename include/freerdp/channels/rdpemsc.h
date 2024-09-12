/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Mouse Cursor Virtual Channel Extension
 *
 * Copyright 2023 Pascal Nowack <Pascal.Nowack@gmx.de>
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

#ifndef FREERDP_CHANNEL_RDPEMSC_H
#define FREERDP_CHANNEL_RDPEMSC_H

/** @defgroup channel_rdpemsc [MS-RDPEMSC]
 * @{
 */

/** \file @code [MS-RDPEMSC] @endcode Mouse Cursor Virtual Channel Extension
 *  \link
 * https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-rdpemsc/2591b507-cd5a-4537-be29-b45540543dc8
 *  \since version 3.0.0
 */
#include <freerdp/api.h>
#include <freerdp/dvc.h>
#include <freerdp/types.h>

/** The command line name of the channel
 *
 *  \since version 3.0.0
 */
#define RDPEMSC_CHANNEL_NAME "mousecursor"
#define RDPEMSC_DVC_CHANNEL_NAME "Microsoft::Windows::RDS::MouseCursor"

#ifdef __cplusplus
extern "C"
{
#endif

	typedef enum
	{
		PDUTYPE_EMSC_RESERVED = 0x00,
		PDUTYPE_CS_CAPS_ADVERTISE = 0x01,
		PDUTYPE_SC_CAPS_CONFIRM = 0x02,
		PDUTYPE_SC_MOUSEPTR_UPDATE = 0x03,
	} RDP_MOUSE_CURSOR_PDUTYPE;

	typedef enum
	{
		TS_UPDATETYPE_MOUSEPTR_SYSTEM_NULL = 0x05,
		TS_UPDATETYPE_MOUSEPTR_SYSTEM_DEFAULT = 0x06,
		TS_UPDATETYPE_MOUSEPTR_POSITION = 0x08,
		TS_UPDATETYPE_MOUSEPTR_CACHED = 0x0A,
		TS_UPDATETYPE_MOUSEPTR_POINTER = 0x0B,
		TS_UPDATETYPE_MOUSEPTR_LARGE_POINTER = 0x0C,
	} TS_UPDATETYPE_MOUSEPTR;

#define RDPEMSC_HEADER_SIZE 4

	typedef struct
	{
		RDP_MOUSE_CURSOR_PDUTYPE pduType;
		TS_UPDATETYPE_MOUSEPTR updateType;
		UINT16 reserved;
	} RDP_MOUSE_CURSOR_HEADER;

	typedef enum
	{
		RDP_MOUSE_CURSOR_CAPVERSION_INVALID = 0x00000000, /** @since version 3.3.0 */
		RDP_MOUSE_CURSOR_CAPVERSION_1 = 0x00000001,
	} RDP_MOUSE_CURSOR_CAPVERSION;

	typedef struct
	{
		UINT32 signature;
		RDP_MOUSE_CURSOR_CAPVERSION version;
		UINT32 size;
	} RDP_MOUSE_CURSOR_CAPSET;

	typedef struct
	{
		RDP_MOUSE_CURSOR_CAPSET capsetHeader;
	} RDP_MOUSE_CURSOR_CAPSET_VERSION1;

	typedef struct
	{
		RDP_MOUSE_CURSOR_HEADER header;
		wArrayList* capsSets;
	} RDP_MOUSE_CURSOR_CAPS_ADVERTISE_PDU;

	typedef struct
	{
		RDP_MOUSE_CURSOR_HEADER header;
		RDP_MOUSE_CURSOR_CAPSET* capsSet;
	} RDP_MOUSE_CURSOR_CAPS_CONFIRM_PDU;

	typedef struct
	{
		UINT16 xPos;
		UINT16 yPos;
	} TS_POINT16;

	typedef struct
	{
		UINT16 xorBpp;
		UINT16 cacheIndex;
		TS_POINT16 hotSpot;
		UINT16 width;
		UINT16 height;
		UINT16 lengthAndMask;
		UINT16 lengthXorMask;
		BYTE* xorMaskData;
		BYTE* andMaskData;
		BYTE pad;
	} TS_POINTERATTRIBUTE;

	typedef struct
	{
		UINT16 xorBpp;
		UINT16 cacheIndex;
		TS_POINT16 hotSpot;
		UINT16 width;
		UINT16 height;
		UINT32 lengthAndMask;
		UINT32 lengthXorMask;
		BYTE* xorMaskData;
		BYTE* andMaskData;
		BYTE pad;
	} TS_LARGEPOINTERATTRIBUTE;

	typedef struct
	{
		RDP_MOUSE_CURSOR_HEADER header;
		TS_POINT16* position;
		UINT16* cachedPointerIndex;
		TS_POINTERATTRIBUTE* pointerAttribute;
		TS_LARGEPOINTERATTRIBUTE* largePointerAttribute;
	} RDP_MOUSE_CURSOR_MOUSEPTR_UPDATE_PDU;

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* FREERDP_CHANNEL_RDPEMSC_H */
