/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Multiparty Virtual Channel
 *
 * Copyright 2014 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_CHANNEL_ENCOMSP_H
#define FREERDP_CHANNEL_ENCOMSP_H

#include <freerdp/api.h>
#include <freerdp/types.h>

#define ENCOMSP_SVC_CHANNEL_NAME "encomsp"

typedef struct
{
	UINT16 cchString;
	WCHAR wString[1024];
} ENCOMSP_UNICODE_STRING;

/* Filter Updated PDU Flags */

#define ENCOMSP_FILTER_ENABLED 0x0001

/* Application Created PDU Flags */

#define ENCOMSP_APPLICATION_SHARED 0x0001

/* Window Created PDU Flags */

#define ENCOMSP_WINDOW_SHARED 0x0001

/* Participant Created PDU Flags */

#define ENCOMSP_MAY_VIEW 0x0001
#define ENCOMSP_MAY_INTERACT 0x0002
#define ENCOMSP_IS_PARTICIPANT 0x0004

/* Participant Removed PDU Disconnection Types */

#define ENCOMSP_PARTICIPANT_DISCONNECTION_REASON_APP 0x00000000
#define ENCOMSP_PARTICIPANT_DISCONNECTION_REASON_CLI 0x00000002

/* Change Participant Control Level PDU Flags */

#define ENCOMSP_REQUEST_VIEW 0x0001
#define ENCOMSP_REQUEST_INTERACT 0x0002
#define ENCOMSP_ALLOW_CONTROL_REQUESTS 0x0008

/* PDU Order Types */

#define ODTYPE_FILTER_STATE_UPDATED 0x0001
#define ODTYPE_APP_REMOVED 0x0002
#define ODTYPE_APP_CREATED 0x0003
#define ODTYPE_WND_REMOVED 0x0004
#define ODTYPE_WND_CREATED 0x0005
#define ODTYPE_WND_SHOW 0x0006
#define ODTYPE_PARTICIPANT_REMOVED 0x0007
#define ODTYPE_PARTICIPANT_CREATED 0x0008
#define ODTYPE_PARTICIPANT_CTRL_CHANGED 0x0009
#define ODTYPE_GRAPHICS_STREAM_PAUSED 0x000A
#define ODTYPE_GRAPHICS_STREAM_RESUMED 0x000B

#define DEFINE_ENCOMSP_HEADER_COMMON() \
	UINT16 Type;                       \
	UINT16 Length

#define ENCOMSP_ORDER_HEADER_SIZE 4

typedef struct
{
	DEFINE_ENCOMSP_HEADER_COMMON();
} ENCOMSP_ORDER_HEADER;

typedef struct
{
	DEFINE_ENCOMSP_HEADER_COMMON();

	BYTE Flags;
} ENCOMSP_FILTER_UPDATED_PDU;

typedef struct
{
	DEFINE_ENCOMSP_HEADER_COMMON();

	UINT16 Flags;
	UINT32 AppId;
	ENCOMSP_UNICODE_STRING Name;
} ENCOMSP_APPLICATION_CREATED_PDU;

typedef struct
{
	DEFINE_ENCOMSP_HEADER_COMMON();

	UINT32 AppId;
} ENCOMSP_APPLICATION_REMOVED_PDU;

typedef struct
{
	DEFINE_ENCOMSP_HEADER_COMMON();

	UINT16 Flags;
	UINT32 AppId;
	UINT32 WndId;
	ENCOMSP_UNICODE_STRING Name;
} ENCOMSP_WINDOW_CREATED_PDU;

typedef struct
{
	DEFINE_ENCOMSP_HEADER_COMMON();

	UINT32 WndId;
} ENCOMSP_WINDOW_REMOVED_PDU;

typedef struct
{
	DEFINE_ENCOMSP_HEADER_COMMON();

	UINT32 WndId;
} ENCOMSP_SHOW_WINDOW_PDU;

typedef struct
{
	DEFINE_ENCOMSP_HEADER_COMMON();

	UINT32 ParticipantId;
	UINT32 GroupId;
	UINT16 Flags;
	ENCOMSP_UNICODE_STRING FriendlyName;
} ENCOMSP_PARTICIPANT_CREATED_PDU;

typedef struct
{
	DEFINE_ENCOMSP_HEADER_COMMON();

	UINT32 ParticipantId;
	UINT32 DiscType;
	UINT32 DiscCode;
} ENCOMSP_PARTICIPANT_REMOVED_PDU;

typedef struct
{
	DEFINE_ENCOMSP_HEADER_COMMON();

	UINT16 Flags;
	UINT32 ParticipantId;
} ENCOMSP_CHANGE_PARTICIPANT_CONTROL_LEVEL_PDU;

typedef struct
{
	DEFINE_ENCOMSP_HEADER_COMMON();
} ENCOMSP_GRAPHICS_STREAM_PAUSED_PDU;

typedef struct
{
	DEFINE_ENCOMSP_HEADER_COMMON();
} ENCOMSP_GRAPHICS_STREAM_RESUMED_PDU;

#endif /* FREERDP_CHANNEL_ENCOMSP_H */
