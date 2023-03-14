/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Type Definitions
 *
 * Copyright 2009-2011 Jay Sorg
 * Copyright 2011 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#ifndef FREERDP_TYPES_H
#define FREERDP_TYPES_H

#include <winpr/wtypes.h>
#include <winpr/wtsapi.h>

#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

#ifndef MAX
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#endif

#ifdef __cplusplus
extern "C"
{
#endif

	typedef enum
	{
		CONNECTION_STATE_INITIAL,
		CONNECTION_STATE_NEGO,
		CONNECTION_STATE_NLA,
		CONNECTION_STATE_AAD,
		CONNECTION_STATE_MCS_CREATE_REQUEST,
		CONNECTION_STATE_MCS_CREATE_RESPONSE,
		CONNECTION_STATE_MCS_ERECT_DOMAIN,
		CONNECTION_STATE_MCS_ATTACH_USER,
		CONNECTION_STATE_MCS_ATTACH_USER_CONFIRM,
		CONNECTION_STATE_MCS_CHANNEL_JOIN_REQUEST,
		CONNECTION_STATE_MCS_CHANNEL_JOIN_RESPONSE,
		CONNECTION_STATE_RDP_SECURITY_COMMENCEMENT,
		CONNECTION_STATE_SECURE_SETTINGS_EXCHANGE,
		CONNECTION_STATE_CONNECT_TIME_AUTO_DETECT_REQUEST,
		CONNECTION_STATE_CONNECT_TIME_AUTO_DETECT_RESPONSE,
		CONNECTION_STATE_LICENSING,
		CONNECTION_STATE_MULTITRANSPORT_BOOTSTRAPPING_REQUEST,
		CONNECTION_STATE_MULTITRANSPORT_BOOTSTRAPPING_RESPONSE,
		CONNECTION_STATE_CAPABILITIES_EXCHANGE_DEMAND_ACTIVE,
		CONNECTION_STATE_CAPABILITIES_EXCHANGE_MONITOR_LAYOUT,
		CONNECTION_STATE_CAPABILITIES_EXCHANGE_CONFIRM_ACTIVE,
		CONNECTION_STATE_FINALIZATION_SYNC,
		CONNECTION_STATE_FINALIZATION_COOPERATE,
		CONNECTION_STATE_FINALIZATION_REQUEST_CONTROL,
		CONNECTION_STATE_FINALIZATION_PERSISTENT_KEY_LIST,
		CONNECTION_STATE_FINALIZATION_FONT_LIST,
		CONNECTION_STATE_FINALIZATION_CLIENT_SYNC,
		CONNECTION_STATE_FINALIZATION_CLIENT_COOPERATE,
		CONNECTION_STATE_FINALIZATION_CLIENT_GRANTED_CONTROL,
		CONNECTION_STATE_FINALIZATION_CLIENT_FONT_MAP,
		CONNECTION_STATE_ACTIVE
	} CONNECTION_STATE;

	typedef struct rdp_channels rdpChannels;
	typedef struct rdp_freerdp freerdp;
	typedef struct rdp_context rdpContext;
	typedef struct rdp_freerdp_peer freerdp_peer;
	typedef struct rdp_transport rdpTransport; /* Opaque */

	typedef struct
	{
		BYTE red;
		BYTE green;
		BYTE blue;
	} PALETTE_ENTRY;

	typedef struct
	{
		UINT32 count;
		PALETTE_ENTRY entries[256];
	} rdpPalette;

	typedef struct
	{
		DWORD size;
		void* data[4];
	} RDP_PLUGIN_DATA;

	typedef struct
	{
		INT16 x;
		INT16 y;
		INT16 width;
		INT16 height;
	} RDP_RECT;

	typedef struct
	{
		UINT16 left;
		UINT16 top;
		UINT16 right;
		UINT16 bottom;
	} RECTANGLE_16;

	typedef struct
	{
		UINT32 left;
		UINT32 top;
		UINT32 width;
		UINT32 height;
	} RECTANGLE_32;

	/** @brief type of RDP transport */
	typedef enum
	{
		RDP_TRANSPORT_TCP = 0,
		RDP_TRANSPORT_UDP_R,
		RDP_TRANSPORT_UDP_L
	} RDP_TRANSPORT_TYPE;

#ifdef __cplusplus
}
#endif

/* Plugin events */

#include <freerdp/message.h>
#include <winpr/collections.h>

#endif /* __RDP_TYPES_H */
