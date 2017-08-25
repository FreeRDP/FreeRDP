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
#include <freerdp/autodetect.h>

#include <winpr/sspi.h>
#include <winpr/ntlm.h>


typedef BOOL (*psPeerContextNew)(freerdp_peer* peer, rdpContext* context);
typedef void (*psPeerContextFree)(freerdp_peer* peer, rdpContext* context);

typedef BOOL (*psPeerInitialize)(freerdp_peer* peer);
typedef BOOL (*psPeerGetFileDescriptor)(freerdp_peer* peer, void** rfds, int* rcount);
typedef HANDLE(*psPeerGetEventHandle)(freerdp_peer* peer);
typedef DWORD (*psPeerGetEventHandles)(freerdp_peer* peer, HANDLE* events, DWORD count);
typedef HANDLE(*psPeerGetReceiveEventHandle)(freerdp_peer* peer);
typedef BOOL (*psPeerCheckFileDescriptor)(freerdp_peer* peer);
typedef BOOL (*psPeerIsWriteBlocked)(freerdp_peer* peer);
typedef int (*psPeerDrainOutputBuffer)(freerdp_peer* peer);
typedef BOOL (*psPeerHasMoreToRead)(freerdp_peer* peer);
typedef BOOL (*psPeerClose)(freerdp_peer* peer);
typedef void (*psPeerDisconnect)(freerdp_peer* peer);
typedef BOOL (*psPeerCapabilities)(freerdp_peer* peer);
typedef BOOL (*psPeerPostConnect)(freerdp_peer* peer);
typedef BOOL (*psPeerActivate)(freerdp_peer* peer);
typedef BOOL (*psPeerLogon)(freerdp_peer* peer, SEC_WINNT_AUTH_IDENTITY* identity, BOOL automatic);
typedef BOOL (*psPeerAdjustMonitorsLayout)(freerdp_peer* peer);
typedef BOOL (*psPeerClientCapabilities)(freerdp_peer* peer);

typedef int (*psPeerSendChannelData)(freerdp_peer* peer, UINT16 channelId, BYTE* data, int size);
typedef int (*psPeerReceiveChannelData)(freerdp_peer* peer, UINT16 channelId, BYTE* data, int size,
                                        int flags, int totalSize);

typedef HANDLE(*psPeerVirtualChannelOpen)(freerdp_peer* peer, const char* name, UINT32 flags);
typedef BOOL (*psPeerVirtualChannelClose)(freerdp_peer* peer, HANDLE hChannel);
typedef int (*psPeerVirtualChannelRead)(freerdp_peer* peer, HANDLE hChannel, BYTE* buffer,
                                        UINT32 length);
typedef int (*psPeerVirtualChannelWrite)(freerdp_peer* peer, HANDLE hChannel, BYTE* buffer,
        UINT32 length);
typedef void* (*psPeerVirtualChannelGetData)(freerdp_peer* peer, HANDLE hChannel);
typedef int (*psPeerVirtualChannelSetData)(freerdp_peer* peer, HANDLE hChannel, void* data);


struct rdp_freerdp_peer
{
	rdpContext* context;

	int sockfd;
	char hostname[50];

	rdpInput* input;
	rdpUpdate* update;
	rdpSettings* settings;
	rdpAutoDetect* autodetect;

	void* ContextExtra;
	size_t ContextSize;
	psPeerContextNew ContextNew;
	psPeerContextFree ContextFree;

	psPeerInitialize Initialize;
	psPeerGetFileDescriptor GetFileDescriptor;
	psPeerGetEventHandle GetEventHandle;
	psPeerGetReceiveEventHandle GetReceiveEventHandle;
	psPeerCheckFileDescriptor CheckFileDescriptor;
	psPeerClose Close;
	psPeerDisconnect Disconnect;

	psPeerCapabilities Capabilities;
	psPeerPostConnect PostConnect;
	psPeerActivate Activate;
	psPeerLogon Logon;

	psPeerSendChannelData SendChannelData;
	psPeerReceiveChannelData ReceiveChannelData;

	psPeerVirtualChannelOpen VirtualChannelOpen;
	psPeerVirtualChannelClose VirtualChannelClose;
	psPeerVirtualChannelRead VirtualChannelRead;
	psPeerVirtualChannelWrite VirtualChannelWrite;
	psPeerVirtualChannelGetData VirtualChannelGetData;
	psPeerVirtualChannelSetData VirtualChannelSetData;

	int pId;
	UINT32 ack_frame_id;
	BOOL local;
	BOOL connected;
	BOOL activated;
	BOOL authenticated;
	SEC_WINNT_AUTH_IDENTITY identity;

	psPeerIsWriteBlocked IsWriteBlocked;
	psPeerDrainOutputBuffer DrainOutputBuffer;
	psPeerHasMoreToRead HasMoreToRead;
	psPeerGetEventHandles GetEventHandles;
	psPeerAdjustMonitorsLayout AdjustMonitorsLayout;
	psPeerClientCapabilities ClientCapabilities;
	psPeerComputeNtlmHash ComputeNtlmHash;
};

#ifdef __cplusplus
extern "C" {
#endif

FREERDP_API BOOL freerdp_peer_context_new(freerdp_peer* client);
FREERDP_API void freerdp_peer_context_free(freerdp_peer* client);

FREERDP_API freerdp_peer* freerdp_peer_new(int sockfd);
FREERDP_API void freerdp_peer_free(freerdp_peer* client);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_PEER_H */
