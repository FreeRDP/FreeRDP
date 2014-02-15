/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Server Channels
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

#ifndef FREERDP_CORE_SERVER_H
#define FREERDP_CORE_SERVER_H

#include <freerdp/freerdp.h>
#include <freerdp/utils/list.h>
#include <freerdp/utils/debug.h>
#include <freerdp/channels/wtsvc.h>

#include <winpr/synch.h>
#include <winpr/stream.h>
#include <winpr/collections.h>

#include "rdp.h"

#define CREATE_REQUEST_PDU			0x01
#define DATA_FIRST_PDU				0x02
#define DATA_PDU				0x03
#define CLOSE_REQUEST_PDU			0x04
#define CAPABILITY_REQUEST_PDU			0x05

enum
{
	RDP_PEER_CHANNEL_TYPE_SVC = 0,
	RDP_PEER_CHANNEL_TYPE_DVC = 1
};

enum
{
	DRDYNVC_STATE_NONE = 0,
	DRDYNVC_STATE_INITIALIZED = 1,
	DRDYNVC_STATE_READY = 2
};

enum
{
	DVC_OPEN_STATE_NONE = 0,
	DVC_OPEN_STATE_SUCCEEDED = 1,
	DVC_OPEN_STATE_FAILED = 2,
	DVC_OPEN_STATE_CLOSED = 3
};

typedef struct rdp_peer_channel rdpPeerChannel;

struct rdp_peer_channel
{
	WTSVirtualChannelManager* vcm;
	freerdp_peer* client;
	UINT32 channelId;
	UINT16 channelType;
	UINT16 index;

	wStream* receiveData;
	wMessagePipe* MsgPipe;

	BYTE dvc_open_state;
	UINT32 dvc_total_length;
};

struct WTSVirtualChannelManager
{
	rdpRdp* rdp;
	freerdp_peer* client;

	wMessagePipe* MsgPipe;

	rdpPeerChannel* drdynvc_channel;
	BYTE drdynvc_state;
	UINT32 dvc_channel_id_seq;
	LIST* dvc_channel_list;
};

#endif /* FREERDP_CORE_SERVER_H */
