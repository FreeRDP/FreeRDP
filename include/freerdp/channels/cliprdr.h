/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Clipboard Virtual Channel Extension
 *
 * Copyright 2013 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_CHANNEL_CLIPRDR_H
#define FREERDP_CHANNEL_CLIPRDR_H

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/utils/cliprdr_utils.h>

#include <winpr/shell.h>

#define CLIPRDR_SVC_CHANNEL_NAME "cliprdr"

/**
 * Clipboard Formats
 */

typedef enum
{
	CB_FORMAT_HTML = 0xD010,
	CB_FORMAT_PNG = 0xD011,
	CB_FORMAT_JPEG = 0xD012,
	CB_FORMAT_GIF = 0xD013,
	CB_FORMAT_TEXTURILIST = 0xD014,
	CB_FORMAT_GNOMECOPIEDFILES = 0xD015,
	CB_FORMAT_MATECOPIEDFILES = 0xD016
} CliprdrFormatType;

/* CLIPRDR_HEADER.msgType */
typedef enum
{
	CB_MONITOR_READY = 0x0001,
	CB_FORMAT_LIST = 0x0002,
	CB_FORMAT_LIST_RESPONSE = 0x0003,
	CB_FORMAT_DATA_REQUEST = 0x0004,
	CB_FORMAT_DATA_RESPONSE = 0x0005,
	CB_TEMP_DIRECTORY = 0x0006,
	CB_CLIP_CAPS = 0x0007,
	CB_FILECONTENTS_REQUEST = 0x0008,
	CB_FILECONTENTS_RESPONSE = 0x0009,
	CB_LOCK_CLIPDATA = 0x000A,
	CB_UNLOCK_CLIPDATA = 0x000B
} CliprdrMsgType;

/* CLIPRDR_HEADER.msgFlags */
#define CB_RESPONSE_OK 0x0001
#define CB_RESPONSE_FAIL 0x0002
#define CB_ASCII_NAMES 0x0004

/* CLIPRDR_CAPS_SET.capabilitySetType */
#define CB_CAPSTYPE_GENERAL 0x0001

/* CLIPRDR_GENERAL_CAPABILITY.lengthCapability */
#define CB_CAPSTYPE_GENERAL_LEN 12

/* CLIPRDR_GENERAL_CAPABILITY.version */
#define CB_CAPS_VERSION_1 0x00000001
#define CB_CAPS_VERSION_2 0x00000002

/* CLIPRDR_GENERAL_CAPABILITY.generalFlags */
#define CB_USE_LONG_FORMAT_NAMES 0x00000002
#define CB_STREAM_FILECLIP_ENABLED 0x00000004
#define CB_FILECLIP_NO_FILE_PATHS 0x00000008
#define CB_CAN_LOCK_CLIPDATA 0x00000010
#define CB_HUGE_FILE_SUPPORT_ENABLED 0x00000020

/* File Contents Request Flags */
#define FILECONTENTS_SIZE 0x00000001
#define FILECONTENTS_RANGE 0x00000002

#ifdef __cplusplus
extern "C"
{
#endif

	/* Special Clipboard Response Formats */

	typedef struct
	{
		UINT32 mappingMode;
		UINT32 xExt;
		UINT32 yExt;
		UINT32 metaFileSize;
		BYTE* metaFileData;
	} CLIPRDR_MFPICT;

	/* Clipboard Messages */

	typedef struct
	{
		UINT16 msgType;
		UINT16 msgFlags;
		UINT32 dataLen;
	} CLIPRDR_HEADER;

	typedef struct
	{
		UINT16 capabilitySetType;
		UINT16 capabilitySetLength;
	} CLIPRDR_CAPABILITY_SET;

	typedef struct
	{
		UINT16 capabilitySetType;
		UINT16 capabilitySetLength;

		UINT32 version;
		UINT32 generalFlags;
	} CLIPRDR_GENERAL_CAPABILITY_SET;

	typedef struct
	{
		CLIPRDR_HEADER common;

		UINT32 cCapabilitiesSets;
		CLIPRDR_CAPABILITY_SET* capabilitySets;
	} CLIPRDR_CAPABILITIES;

	typedef struct
	{
		CLIPRDR_HEADER common;
	} CLIPRDR_MONITOR_READY;

	typedef struct
	{
		CLIPRDR_HEADER common;

		char szTempDir[520];
	} CLIPRDR_TEMP_DIRECTORY;

	typedef struct
	{
		UINT32 formatId;
		char* formatName;
	} CLIPRDR_FORMAT;

	typedef struct
	{
		CLIPRDR_HEADER common;

		UINT32 numFormats;
		CLIPRDR_FORMAT* formats;
	} CLIPRDR_FORMAT_LIST;

	typedef struct
	{
		CLIPRDR_HEADER common;
	} CLIPRDR_FORMAT_LIST_RESPONSE;

	typedef struct
	{
		CLIPRDR_HEADER common;

		UINT32 clipDataId;
	} CLIPRDR_LOCK_CLIPBOARD_DATA;

	typedef struct
	{
		CLIPRDR_HEADER common;

		UINT32 clipDataId;
	} CLIPRDR_UNLOCK_CLIPBOARD_DATA;

	typedef struct
	{
		CLIPRDR_HEADER common;

		UINT32 requestedFormatId;
	} CLIPRDR_FORMAT_DATA_REQUEST;

	typedef struct
	{
		CLIPRDR_HEADER common;

		const BYTE* requestedFormatData;
	} CLIPRDR_FORMAT_DATA_RESPONSE;

	typedef struct
	{
		CLIPRDR_HEADER common;

		UINT32 streamId;
		UINT32 listIndex;
		UINT32 dwFlags;
		UINT32 nPositionLow;
		UINT32 nPositionHigh;
		UINT32 cbRequested;
		BOOL haveClipDataId;
		UINT32 clipDataId;
	} CLIPRDR_FILE_CONTENTS_REQUEST;

	typedef struct
	{
		CLIPRDR_HEADER common;

		UINT32 streamId;
		UINT32 cbRequested;
		const BYTE* requestedData;
	} CLIPRDR_FILE_CONTENTS_RESPONSE;

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CHANNEL_CLIPRDR_H */
