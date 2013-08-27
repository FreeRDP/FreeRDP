/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * RDP Server Peer
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

#ifndef FREERDP_PEER_H
#define FREERDP_PEER_H

#include <freerdp/api.h>
#include <freerdp/types.h>
#include <freerdp/settings.h>
#include <freerdp/input.h>
#include <freerdp/update.h>

#include <winpr/sspi.h>

typedef void (*psPeerContextNew)(freerdp_peer* client, rdpContext* context);
typedef void (*psPeerContextFree)(freerdp_peer* client, rdpContext* context);

typedef BOOL (*psPeerInitialize)(freerdp_peer* client);
typedef BOOL (*psPeerGetFileDescriptor)(freerdp_peer* client, void** rfds, int* rcount);
typedef HANDLE (*psPeerGetEventHandle)(freerdp_peer* client);
typedef BOOL (*psPeerCheckFileDescriptor)(freerdp_peer* client);
typedef BOOL (*psPeerClose)(freerdp_peer* client);
typedef void (*psPeerDisconnect)(freerdp_peer* client);
typedef BOOL (*psPeerCapabilities)(freerdp_peer* client);
typedef BOOL (*psPeerPostConnect)(freerdp_peer* client);
typedef BOOL (*psPeerActivate)(freerdp_peer* client);
typedef BOOL (*psPeerLogon)(freerdp_peer* client, SEC_WINNT_AUTH_IDENTITY* identity, BOOL automatic);

typedef int (*psPeerSendChannelData)(freerdp_peer* client, int channelId, BYTE* data, int size);
typedef int (*psPeerReceiveChannelData)(freerdp_peer* client, int channelId, BYTE* data, int size, int flags, int total_size);

struct rdp_freerdp_peer
{
	rdpContext* context;
	int sockfd;
	char hostname[50];

	rdpInput* input;
	rdpUpdate* update;
	rdpSettings* settings;

	size_t ContextSize;
	psPeerContextNew ContextNew;
	psPeerContextFree ContextFree;

	psPeerInitialize Initialize;
	psPeerGetFileDescriptor GetFileDescriptor;
	psPeerGetEventHandle GetEventHandle;
	psPeerCheckFileDescriptor CheckFileDescriptor;
	psPeerClose Close;
	psPeerDisconnect Disconnect;

	psPeerCapabilities Capabilities;
	psPeerPostConnect PostConnect;
	psPeerActivate Activate;
	psPeerLogon Logon;

	psPeerSendChannelData SendChannelData;
	psPeerReceiveChannelData ReceiveChannelData;

	int pId;
	UINT32 ack_frame_id;
	BOOL local;
	BOOL connected;
	BOOL activated;
	BOOL authenticated;
	SEC_WINNT_AUTH_IDENTITY identity;
};

#ifdef __cplusplus
extern "C" {
#endif

FREERDP_API void freerdp_peer_context_new(freerdp_peer* client);
FREERDP_API void freerdp_peer_context_free(freerdp_peer* client);

FREERDP_API freerdp_peer* freerdp_peer_new(int sockfd);
FREERDP_API void freerdp_peer_free(freerdp_peer* client);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_PEER_H */
