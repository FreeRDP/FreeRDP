/**
 * FreeRDP: A Remote Desktop Protocol client.
 * RDP Server Listener
 *
 * Copyright 2011 Vic Lee
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

#ifndef __FREERDP_LISTENER_H
#define __FREERDP_LISTENER_H

typedef struct rdp_freerdp_listener freerdp_listener;

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/settings.h>
#include <freerdp/peer.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef boolean (*psListenerOpen)(freerdp_listener* instance, const char* bind_address, uint16 port);
typedef boolean (*psListenerGetFileDescriptor)(freerdp_listener* instance, void** rfds, int* rcount);
typedef boolean (*psListenerCheckFileDescriptor)(freerdp_listener* instance);
typedef void (*psListenerClose)(freerdp_listener* instance);
typedef void (*psPeerAccepted)(freerdp_listener* instance, freerdp_peer* client);

struct rdp_freerdp_listener
{
	void* info;
	void* listener;
	void* param1;
	void* param2;
	void* param3;
	void* param4;

	psListenerOpen Open;
	psListenerGetFileDescriptor GetFileDescriptor;
	psListenerCheckFileDescriptor CheckFileDescriptor;
	psListenerClose Close;

	psPeerAccepted PeerAccepted;
};

FREERDP_API freerdp_listener* freerdp_listener_new(void);
FREERDP_API void freerdp_listener_free(freerdp_listener* instance);

#ifdef __cplusplus
}
#endif

#endif

