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

#define CLIPRDR_SVC_CHANNEL_NAME	"cliprdr"

/**
 * Clipboard Formats
 */

#define CB_FORMAT_HTML			0xD010
#define CB_FORMAT_PNG			0xD011
#define CB_FORMAT_JPEG			0xD012
#define CB_FORMAT_GIF			0xD013

/* CLIPRDR_HEADER.msgType */
#define CB_MONITOR_READY		0x0001
#define CB_FORMAT_LIST			0x0002
#define CB_FORMAT_LIST_RESPONSE		0x0003
#define CB_FORMAT_DATA_REQUEST		0x0004
#define CB_FORMAT_DATA_RESPONSE		0x0005
#define CB_TEMP_DIRECTORY		0x0006
#define CB_CLIP_CAPS			0x0007
#define CB_FILECONTENTS_REQUEST		0x0008
#define CB_FILECONTENTS_RESPONSE	0x0009
#define CB_LOCK_CLIPDATA		0x000A
#define CB_UNLOCK_CLIPDATA		0x000B

/* CLIPRDR_HEADER.msgFlags */
#define CB_RESPONSE_OK			0x0001
#define CB_RESPONSE_FAIL		0x0002
#define CB_ASCII_NAMES			0x0004

/* CLIPRDR_CAPS_SET.capabilitySetType */
#define CB_CAPSTYPE_GENERAL		0x0001

/* CLIPRDR_GENERAL_CAPABILITY.lengthCapability */
#define CB_CAPSTYPE_GENERAL_LEN		12

/* CLIPRDR_GENERAL_CAPABILITY.version */
#define CB_CAPS_VERSION_1		0x00000001
#define CB_CAPS_VERSION_2		0x00000002

/* CLIPRDR_GENERAL_CAPABILITY.generalFlags */
#define CB_USE_LONG_FORMAT_NAMES	0x00000002
#define CB_STREAM_FILECLIP_ENABLED	0x00000004
#define CB_FILECLIP_NO_FILE_PATHS	0x00000008
#define CB_CAN_LOCK_CLIPDATA		0x00000010

/* File Contents Request Flags */
#define FILECONTENTS_SIZE		0x00000001
#define FILECONTENTS_RANGE		0x00000002

/* Special Clipboard Response Formats */

struct _CLIPRDR_MFPICT
{
	UINT32 mappingMode;
	UINT32 xExt;
	UINT32 yExt;
	UINT32 metaFileSize;
	BYTE* metaFileData;
};
typedef struct _CLIPRDR_MFPICT CLIPRDR_MFPICT;

struct _CLIPRDR_FILEDESCRIPTOR
{
	DWORD    dwFlags;
	BYTE     clsid[16];
	BYTE     sizel[8];
	BYTE     pointl[8];
	DWORD    dwFileAttributes;
	FILETIME ftCreationTime;
	FILETIME ftLastAccessTime;
	FILETIME ftLastWriteTime;
	DWORD    nFileSizeHigh;
	DWORD    nFileSizeLow;
	union
	{
		WCHAR    w[260];
		CHAR	 c[520];
	} cFileName;
};
typedef struct _CLIPRDR_FILEDESCRIPTOR CLIPRDR_FILEDESCRIPTOR;

struct _CLIPRDR_FILELIST
{
	UINT32 cItems;
	CLIPRDR_FILEDESCRIPTOR* fileDescriptorArray;
};
typedef struct _CLIPRDR_FILELIST CLIPRDR_FILELIST;

/* Clipboard Messages */

#define DEFINE_CLIPRDR_HEADER_COMMON() \
	UINT16 msgType; \
	UINT16 msgFlags; \
	UINT32 dataLen

struct _CLIPRDR_HEADER
{
	DEFINE_CLIPRDR_HEADER_COMMON();
};
typedef struct _CLIPRDR_HEADER CLIPRDR_HEADER;

struct _CLIPRDR_CAPABILITY_SET
{
	UINT16 capabilitySetType;
	UINT16 capabilitySetLength;
};
typedef struct _CLIPRDR_CAPABILITY_SET CLIPRDR_CAPABILITY_SET;

struct _CLIPRDR_GENERAL_CAPABILITY_SET
{
	UINT16 capabilitySetType;
	UINT16 capabilitySetLength;

	UINT32 version;
	UINT32 generalFlags;
};
typedef struct _CLIPRDR_GENERAL_CAPABILITY_SET CLIPRDR_GENERAL_CAPABILITY_SET;

struct _CLIPRDR_CAPABILITIES
{
	DEFINE_CLIPRDR_HEADER_COMMON();

	UINT32 cCapabilitiesSets;
	CLIPRDR_CAPABILITY_SET* capabilitySets;
};
typedef struct _CLIPRDR_CAPABILITIES CLIPRDR_CAPABILITIES;

struct _CLIPRDR_MONITOR_READY
{
	DEFINE_CLIPRDR_HEADER_COMMON();
};
typedef struct _CLIPRDR_MONITOR_READY CLIPRDR_MONITOR_READY;

struct _CLIPRDR_TEMP_DIRECTORY
{
	DEFINE_CLIPRDR_HEADER_COMMON();

	char szTempDir[520];
};
typedef struct _CLIPRDR_TEMP_DIRECTORY CLIPRDR_TEMP_DIRECTORY;

struct _CLIPRDR_FORMAT
{
	UINT32 formatId;
	char* formatName;
};
typedef struct _CLIPRDR_FORMAT CLIPRDR_FORMAT;

struct _CLIPRDR_FORMAT_LIST
{
	DEFINE_CLIPRDR_HEADER_COMMON();

	UINT32 numFormats;
	CLIPRDR_FORMAT* formats;
};
typedef struct _CLIPRDR_FORMAT_LIST CLIPRDR_FORMAT_LIST;

struct _CLIPRDR_FORMAT_LIST_RESPONSE
{
	DEFINE_CLIPRDR_HEADER_COMMON();
};
typedef struct _CLIPRDR_FORMAT_LIST_RESPONSE CLIPRDR_FORMAT_LIST_RESPONSE;

struct _CLIPRDR_LOCK_CLIPBOARD_DATA
{
	DEFINE_CLIPRDR_HEADER_COMMON();

	UINT32 clipDataId;
};
typedef struct _CLIPRDR_LOCK_CLIPBOARD_DATA CLIPRDR_LOCK_CLIPBOARD_DATA;

struct _CLIPRDR_UNLOCK_CLIPBOARD_DATA
{
	DEFINE_CLIPRDR_HEADER_COMMON();

	UINT32 clipDataId;
};
typedef struct _CLIPRDR_UNLOCK_CLIPBOARD_DATA CLIPRDR_UNLOCK_CLIPBOARD_DATA;

struct _CLIPRDR_FORMAT_DATA_REQUEST
{
	DEFINE_CLIPRDR_HEADER_COMMON();

	UINT32 requestedFormatId;
};
typedef struct _CLIPRDR_FORMAT_DATA_REQUEST CLIPRDR_FORMAT_DATA_REQUEST;

struct _CLIPRDR_FORMAT_DATA_RESPONSE
{
	DEFINE_CLIPRDR_HEADER_COMMON();

	BYTE* requestedFormatData;
};
typedef struct _CLIPRDR_FORMAT_DATA_RESPONSE CLIPRDR_FORMAT_DATA_RESPONSE;

struct _CLIPRDR_FILE_CONTENTS_REQUEST
{
	DEFINE_CLIPRDR_HEADER_COMMON();

	UINT32 streamId;
	UINT32 listIndex;
	UINT32 dwFlags;
	UINT32 nPositionLow;
	UINT32 nPositionHigh;
	UINT32 cbRequested;
	UINT32 clipDataId;
};
typedef struct _CLIPRDR_FILE_CONTENTS_REQUEST CLIPRDR_FILE_CONTENTS_REQUEST;

struct _CLIPRDR_FILE_CONTENTS_RESPONSE
{
	DEFINE_CLIPRDR_HEADER_COMMON();

	UINT32 streamId;
	UINT32 dwFlags;
	UINT32 cbRequested;
	BYTE* requestedData;
};
typedef struct _CLIPRDR_FILE_CONTENTS_RESPONSE CLIPRDR_FILE_CONTENTS_RESPONSE;

#endif /* FREERDP_CHANNEL_CLIPRDR_H */

