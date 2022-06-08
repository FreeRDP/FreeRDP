/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Extended Input channel server-side definitions
 *
 * Copyright 2014 Thincast Technologies Gmbh.
 * Copyright 2014 David FORT <contact@hardening-consulting.com>
 * Copyright 2015 Thincast Technologies GmbH
 * Copyright 2015 DI (FH) Martin Haimberger <martin.haimberger@thincast.com>
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

#ifndef FREERDP_CHANNEL_RDPEI_SERVER_H
#define FREERDP_CHANNEL_RDPEI_SERVER_H

#include <freerdp/channels/wtsvc.h>
#include <freerdp/channels/rdpei.h>

typedef struct s_rdpei_server_context RdpeiServerContext;
typedef struct s_rdpei_server_private RdpeiServerPrivate;

struct s_rdpei_server_context
{
	HANDLE vcm;

	RdpeiServerPrivate* priv;

	UINT32 clientVersion;
	UINT16 maxTouchPoints;
	UINT32 protocolFlags;

	/** callbacks that can be set by the user */
	UINT (*onClientReady)(RdpeiServerContext* context);
	UINT (*onTouchEvent)(RdpeiServerContext* context, const RDPINPUT_TOUCH_EVENT* touchEvent);
	UINT (*onPenEvent)(RdpeiServerContext* context, const RDPINPUT_PEN_EVENT* penEvent);
	UINT (*onTouchReleased)(RdpeiServerContext* context, BYTE contactId);

	void* user_data; /* user data, useful for callbacks */

	/**
	 * Callback, when the channel got its id assigned.
	 */
	BOOL (*onChannelIdAssigned)(RdpeiServerContext* context, UINT32 channelId);
};

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API RdpeiServerContext* rdpei_server_context_new(HANDLE vcm);
	FREERDP_API void rdpei_server_context_reset(RdpeiServerContext* context);
	FREERDP_API void rdpei_server_context_free(RdpeiServerContext* context);
	FREERDP_API HANDLE rdpei_server_get_event_handle(RdpeiServerContext* context);
	FREERDP_API UINT rdpei_server_init(RdpeiServerContext* context);
	FREERDP_API UINT rdpei_server_handle_messages(RdpeiServerContext* context);

	FREERDP_API UINT rdpei_server_send_sc_ready(RdpeiServerContext* context, UINT32 version,
	                                            UINT32 features);
	FREERDP_API UINT rdpei_server_suspend(RdpeiServerContext* context);
	FREERDP_API UINT rdpei_server_resume(RdpeiServerContext* context);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_CHANNEL_RDPEI_SERVER_H */
