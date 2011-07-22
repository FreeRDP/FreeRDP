/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Connection Sequence
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

#ifndef __CONNECTION_H
#define __CONNECTION_H

#include "rdp.h"
#include "tpkt.h"
#include "tpdu.h"
#include "nego.h"
#include "mcs.h"
#include "transport.h"

#include <freerdp/settings.h>
#include <freerdp/utils/memory.h>

#define SYNCMSGTYPE_SYNC		0x0001

#define CTRLACTION_REQUEST_CONTROL	0x0001
#define CTRLACTION_GRANTED_CONTROL	0x0002
#define CTRLACTION_DETACH		0x0003
#define CTRLACTION_COOPERATE		0x0004

#define PERSIST_FIRST_PDU		0x01
#define PERSIST_LAST_PDU		0x02

#define FONTLIST_FIRST			0x0001
#define FONTLIST_LAST			0x0002

boolean rdp_client_connect(rdpRdp* rdp);

void rdp_send_client_synchronize_pdu(rdpRdp* rdp);
void rdp_send_client_control_pdu(rdpRdp* rdp, uint16 action);
void rdp_send_client_persistent_key_list_pdu(rdpRdp* rdp);
void rdp_send_client_font_list_pdu(rdpRdp* rdp, uint16 flags);

#endif /* __CONNECTION_H */
