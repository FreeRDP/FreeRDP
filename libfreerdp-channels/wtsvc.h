/**
 * FreeRDP: A Remote Desktop Protocol client.
 * Server Virtual Channel Interface
 *
 * Copyright 2011-2012 Vic Lee
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

#ifndef __WTSVC_H
#define __WTSVC_H

#include <freerdp/freerdp.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/list.h>
#include <freerdp/utils/mutex.h>
#include <freerdp/utils/wait_obj.h>
#include <freerdp/channels/wtsvc.h>

enum
{
	RDP_PEER_CHANNEL_TYPE_SVC = 0,
	RDP_PEER_CHANNEL_TYPE_DVC = 1,
	RDP_PEER_CHANNEL_TYPE_DVC_SUB = 2
};

typedef struct rdp_peer_channel
{
	WTSVirtualChannelManager* vcm;
	freerdp_peer* client;
	uint16 channel_id;
	uint16 channel_type;
	uint16 index;

	STREAM* receive_data;
	struct wait_obj* receive_event;
	LIST* receive_queue;
	freerdp_mutex mutex;
} rdpPeerChannel;

struct WTSVirtualChannelManager
{
	freerdp_peer* client;
	struct wait_obj* send_event;
	LIST* send_queue;
	freerdp_mutex mutex;

	rdpPeerChannel* drdynvc_channel;
};

#endif /* __WTSVC_H */
