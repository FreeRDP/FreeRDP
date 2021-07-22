/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * FreeRDP Dynamic Virtual Channel Passthrough
 *
 * Copyright 2021 Alexandru Bagu <alexandru.bagu@gmail.com>
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

#ifndef FREERDP_SERVER_DYNAMIC_PASSTHROUGH_H
#define FREERDP_SERVER_DYNAMIC_PASSTHROUGH_H

typedef struct _dynamic_passthrough_server_context DynamicPassthroughServerContext;
typedef struct _dynamic_passthrough_server_private DynamicPassthroughServerPrivate;

typedef UINT (*pcDynamicPassthroughServerSend)(DynamicPassthroughServerContext* context,
                                               const wStream* stream);

typedef UINT (*pcDynamicPassthroughServerOnReceive)(DynamicPassthroughServerContext* context,
                                                    const wStream* stream);


struct _dynamic_passthrough_server_context
{
	void* custom;
	const char* channelname;
	void* client;
	DynamicPassthroughServerPrivate* priv;

	pcDynamicPassthroughServerSend Send;
	pcDynamicPassthroughServerOnReceive OnReceive;
};

struct _dynamic_passthrough_server_private
{
	BOOL isReady;
	HANDLE channelEvent;
	HANDLE thread;
	HANDLE stopEvent;

	void* channel;
};

#endif /*FREERDP_SERVER_DYNAMIC_PASSTHROUGH_H*/
