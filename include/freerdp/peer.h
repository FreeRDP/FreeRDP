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
#include <winpr/winsock.h>

typedef BOOL (*psPeerContextNew)(freerdp_peer* peer, rdpContext* context);
typedef void (*psPeerContextFree)(freerdp_peer* peer, rdpContext* context);

typedef BOOL (*psPeerInitialize)(freerdp_peer* peer);
#if defined(WITH_FREERDP_DEPRECATED)
WINPR_DEPRECATED_VAR("Use psPeerGetEventHandle instead",
                     typedef BOOL (*psPeerGetFileDescriptor)(freerdp_peer* peer, void** rfds,
                                                             int* rcount);)
#endif
typedef HANDLE (*psPeerGetEventHandle)(freerdp_peer* peer);
typedef DWORD (*psPeerGetEventHandles)(freerdp_peer* peer, HANDLE* events, DWORD count);
typedef HANDLE (*psPeerGetReceiveEventHandle)(freerdp_peer* peer);
typedef BOOL (*psPeerCheckFileDescriptor)(freerdp_peer* peer);
typedef BOOL (*psPeerIsWriteBlocked)(freerdp_peer* peer);
typedef int (*psPeerDrainOutputBuffer)(freerdp_peer* peer);
typedef BOOL (*psPeerHasMoreToRead)(freerdp_peer* peer);
typedef BOOL (*psPeerClose)(freerdp_peer* peer);
typedef void (*psPeerDisconnect)(freerdp_peer* peer);
typedef BOOL (*psPeerCapabilities)(freerdp_peer* peer);
typedef BOOL (*psPeerPostConnect)(freerdp_peer* peer);
typedef BOOL (*psPeerActivate)(freerdp_peer* peer);
typedef BOOL (*psPeerLogon)(freerdp_peer* peer, const SEC_WINNT_AUTH_IDENTITY* identity,
                            BOOL automatic);
typedef BOOL (*psPeerAdjustMonitorsLayout)(freerdp_peer* peer);
typedef BOOL (*psPeerClientCapabilities)(freerdp_peer* peer);

typedef BOOL (*psPeerSendChannelData)(freerdp_peer* peer, UINT16 channelId, const BYTE* data,
                                      size_t size);
typedef BOOL (*psPeerSendChannelPacket)(freerdp_peer* client, UINT16 channelId, size_t totalSize,
                                        UINT32 flags, const BYTE* data, size_t chunkSize);
typedef BOOL (*psPeerReceiveChannelData)(freerdp_peer* peer, UINT16 channelId, const BYTE* data,
                                         size_t size, UINT32 flags, size_t totalSize);

typedef HANDLE (*psPeerVirtualChannelOpen)(freerdp_peer* peer, const char* name, UINT32 flags);
typedef BOOL (*psPeerVirtualChannelClose)(freerdp_peer* peer, HANDLE hChannel);
typedef int (*psPeerVirtualChannelRead)(freerdp_peer* peer, HANDLE hChannel, BYTE* buffer,
                                        UINT32 length);
typedef int (*psPeerVirtualChannelWrite)(freerdp_peer* peer, HANDLE hChannel, const BYTE* buffer,
                                         UINT32 length);
typedef void* (*psPeerVirtualChannelGetData)(freerdp_peer* peer, HANDLE hChannel);
typedef int (*psPeerVirtualChannelSetData)(freerdp_peer* peer, HANDLE hChannel, void* data);
typedef BOOL (*psPeerSetState)(freerdp_peer* peer, CONNECTION_STATE state);
typedef BOOL (*psPeerReachedState)(freerdp_peer* peer, CONNECTION_STATE state);

/** @brief the result of the license callback */
typedef enum
{
	LICENSE_CB_INTERNAL_ERROR, /** an internal error happened in the callback */
	LICENSE_CB_ABORT,          /** licensing process failed, abort the connection */
	LICENSE_CB_IN_PROGRESS, /** incoming packet has been treated, we're waiting for further packets
	                           to complete the workflow */
	LICENSE_CB_COMPLETED    /** the licensing workflow has completed, go to next step */
} LicenseCallbackResult;

typedef LicenseCallbackResult (*psPeerLicenseCallback)(freerdp_peer* peer, wStream* s);

struct rdp_freerdp_peer
{
	ALIGN64 rdpContext* context;

	ALIGN64 int sockfd;
	ALIGN64 char hostname[50];

#if defined(WITH_FREERDP_DEPRECATED)
	WINPR_DEPRECATED_VAR("Use rdpContext::update instead", ALIGN64 rdpUpdate* update;)
	WINPR_DEPRECATED_VAR("Use rdpContext::settings instead", ALIGN64 rdpSettings* settings;)
	WINPR_DEPRECATED_VAR("Use rdpContext::autodetect instead", ALIGN64 rdpAutoDetect* autodetect;)
#else
	UINT64 reservedX[3];
#endif

	ALIGN64 void* ContextExtra;
	ALIGN64 size_t ContextSize;
	ALIGN64 psPeerContextNew ContextNew;
	ALIGN64 psPeerContextFree ContextFree;

	ALIGN64 psPeerInitialize Initialize;
#if defined(WITH_FREERDP_DEPRECATED)
	WINPR_DEPRECATED_VAR("Use freerdp_peer::GetEventHandle instead",
	                     ALIGN64 psPeerGetFileDescriptor GetFileDescriptor;)
#else
	UINT64 reserved;
#endif
	ALIGN64 psPeerGetEventHandle GetEventHandle;
	ALIGN64 psPeerGetReceiveEventHandle GetReceiveEventHandle;
	ALIGN64 psPeerCheckFileDescriptor CheckFileDescriptor;
	ALIGN64 psPeerClose Close;
	ALIGN64 psPeerDisconnect Disconnect;

	ALIGN64 psPeerCapabilities Capabilities;
	ALIGN64 psPeerPostConnect PostConnect;
	ALIGN64 psPeerActivate Activate;
	ALIGN64 psPeerLogon Logon;

	ALIGN64 psPeerSendChannelData SendChannelData;
	ALIGN64 psPeerReceiveChannelData ReceiveChannelData;

	ALIGN64 psPeerVirtualChannelOpen VirtualChannelOpen;
	ALIGN64 psPeerVirtualChannelClose VirtualChannelClose;
	ALIGN64 psPeerVirtualChannelRead VirtualChannelRead;
	ALIGN64 psPeerVirtualChannelWrite VirtualChannelWrite;
	ALIGN64 psPeerVirtualChannelGetData VirtualChannelGetData;
	ALIGN64 psPeerVirtualChannelSetData VirtualChannelSetData;

	ALIGN64 int pId;
	ALIGN64 UINT32 ack_frame_id;
	ALIGN64 BOOL local;
	ALIGN64 BOOL connected;
	ALIGN64 BOOL activated;
	ALIGN64 BOOL authenticated;
	ALIGN64 SEC_WINNT_AUTH_IDENTITY identity;

	ALIGN64 psPeerIsWriteBlocked IsWriteBlocked;
	ALIGN64 psPeerDrainOutputBuffer DrainOutputBuffer;
	ALIGN64 psPeerHasMoreToRead HasMoreToRead;
	ALIGN64 psPeerGetEventHandles GetEventHandles;
	ALIGN64 psPeerAdjustMonitorsLayout AdjustMonitorsLayout;
	ALIGN64 psPeerClientCapabilities ClientCapabilities;
#if defined(WITH_FREERDP_DEPRECATED)
	WINPR_DEPRECATED_VAR("Use freerdp_peer::SspiNtlmHashCallback instead",
	                     ALIGN64 psPeerComputeNtlmHash ComputeNtlmHash;)
#else
	UINT64 reserved2;
#endif
	ALIGN64 psPeerLicenseCallback LicenseCallback;

	ALIGN64 psPeerSendChannelPacket SendChannelPacket;

	/**
	 * @brief SetState Function pointer allowing to manually set the state of the
	 * internal state machine.
	 *
	 * This is useful if certain parts of a RDP connection must be skipped (e.g.
	 * when replaying a RDP connection dump the authentication/negotiate parts
	 * must be skipped)
	 *
	 * \note Must be called after \b Initialize as that also modifies the state.
	 */
	ALIGN64 psPeerSetState SetState;
	ALIGN64 psPeerReachedState ReachedState;
	ALIGN64 psSspiNtlmHashCallback SspiNtlmHashCallback;
};

#ifdef __cplusplus
extern "C"
{
#endif

	FREERDP_API BOOL freerdp_peer_context_new(freerdp_peer* client);
	FREERDP_API BOOL freerdp_peer_context_new_ex(freerdp_peer* client, const rdpSettings* settings);
	FREERDP_API void freerdp_peer_context_free(freerdp_peer* client);

	FREERDP_API freerdp_peer* freerdp_peer_new(int sockfd);
	FREERDP_API void freerdp_peer_free(freerdp_peer* client);
	FREERDP_API BOOL freerdp_peer_set_local_and_hostname(freerdp_peer* client,
	                                                     const struct sockaddr_storage* peer_addr);

#ifdef __cplusplus
}
#endif

#endif /* FREERDP_PEER_H */
