/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Activation Sequence
 *
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

#ifndef FREERDP_LIB_CORE_ACTIVATION_H
#define FREERDP_LIB_CORE_ACTIVATION_H

#include "rdp.h"

#include <freerdp/api.h>
#include <freerdp/settings.h>
#include <freerdp/cache/persistent.h>

#define SYNCMSGTYPE_SYNC 0x0001

typedef enum
{
	CTRLACTION_REQUEST_CONTROL = 0x0001,
	CTRLACTION_GRANTED_CONTROL = 0x0002,
	CTRLACTION_DETACH = 0x0003,
	CTRLACTION_COOPERATE = 0x0004
} CTRLACTION;

struct _RDP_BITMAP_PERSISTENT_INFO
{
	UINT16 numEntriesCache0;
	UINT16 numEntriesCache1;
	UINT16 numEntriesCache2;
	UINT16 numEntriesCache3;
	UINT16 numEntriesCache4;
	UINT16 totalEntriesCache0;
	UINT16 totalEntriesCache1;
	UINT16 totalEntriesCache2;
	UINT16 totalEntriesCache3;
	UINT16 totalEntriesCache4;
	UINT32 keyCount;
	UINT64* keyList;
};
typedef struct _RDP_BITMAP_PERSISTENT_INFO RDP_BITMAP_PERSISTENT_INFO;

#define PERSIST_FIRST_PDU 0x01
#define PERSIST_LAST_PDU 0x02

#define FONTLIST_FIRST 0x0001
#define FONTLIST_LAST 0x0002

FREERDP_LOCAL const char* rdp_ctrlaction_string(UINT16 action, char* buffer, size_t size);
FREERDP_LOCAL BOOL rdp_recv_deactivate_all(rdpRdp* rdp, wStream* s);
FREERDP_LOCAL BOOL rdp_send_deactivate_all(rdpRdp* rdp);

FREERDP_LOCAL BOOL rdp_recv_synchronize_pdu(rdpRdp* rdp, wStream* s);
FREERDP_LOCAL BOOL rdp_send_server_synchronize_pdu(rdpRdp* rdp);
FREERDP_LOCAL BOOL rdp_recv_client_synchronize_pdu(rdpRdp* rdp, wStream* s);
FREERDP_LOCAL BOOL rdp_send_client_synchronize_pdu(rdpRdp* rdp);

FREERDP_LOCAL BOOL rdp_recv_server_control_pdu(rdpRdp* rdp, wStream* s);
FREERDP_LOCAL BOOL rdp_send_server_control_cooperate_pdu(rdpRdp* rdp);
FREERDP_LOCAL BOOL rdp_send_client_control_pdu(rdpRdp* rdp, UINT16 action);
FREERDP_LOCAL BOOL rdp_send_client_persistent_key_list_pdu(rdpRdp* rdp);
FREERDP_LOCAL BOOL rdp_send_client_font_list_pdu(rdpRdp* rdp, UINT16 flags);
FREERDP_LOCAL BOOL rdp_recv_font_map_pdu(rdpRdp* rdp, wStream* s);

FREERDP_LOCAL BOOL rdp_server_accept_client_control_pdu(rdpRdp* rdp, wStream* s);
FREERDP_LOCAL BOOL rdp_server_accept_client_font_list_pdu(rdpRdp* rdp, wStream* s);
FREERDP_LOCAL BOOL rdp_server_accept_client_persistent_key_list_pdu(rdpRdp* rdp, wStream* s);

#endif /* FREERDP_LIB_CORE_ACTIVATION_H */
