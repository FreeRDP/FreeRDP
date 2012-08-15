/**
 * FreeRDP: A Remote Desktop Protocol Client
 * Terminal Server Gateway (TSG)
 *
 * Copyright 2012 Fujitsu Technology Solutions GmbH
 * Copyright 2012 Dmitrij Jasnov <dmitrij.jasnov@ts.fujitsu.com>
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

#ifndef FREERDP_CORE_TSG_H
#define FREERDP_CORE_TSG_H

typedef struct rdp_tsg rdpTsg;

#include "rpc.h"
#include "transport.h"

#include <winpr/rpc.h>
#include <winpr/winpr.h>

#include <time.h>
#include <freerdp/types.h>
#include <freerdp/settings.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/wait_obj.h>
#include <freerdp/utils/debug.h>

struct rdp_tsg
{
	rdpRpc* rpc;
	rdpSettings* settings;
	rdpTransport* transport;
	uint8 TunnelContext[16];
	uint8 ChannelContext[16];
};

typedef wchar_t* RESOURCENAME;

#define MAX_RESOURCE_NAMES			50

typedef struct _tsendpointinfo
{
	RESOURCENAME* resourceName;
	unsigned long numResourceNames;
	RESOURCENAME* alternateResourceNames;
	unsigned short numAlternateResourceNames;
	unsigned long Port;
} TSENDPOINTINFO, *PTSENDPOINTINFO;

#define TSG_PACKET_TYPE_HEADER			0x00004844
#define TSG_PACKET_TYPE_VERSIONCAPS		0x00005643
#define TSG_PACKET_TYPE_QUARCONFIGREQUEST	0x00005143
#define TSG_PACKET_TYPE_QUARREQUEST		0x00005152
#define TSG_PACKET_TYPE_RESPONSE		0x00005052
#define TSG_PACKET_TYPE_QUARENC_RESPONSE	0x00004552
#define TSG_CAPABILITY_TYPE_NAP			0x00000001
#define TSG_PACKET_TYPE_CAPS_RESPONSE		0x00004350
#define TSG_PACKET_TYPE_MSGREQUEST_PACKET	0x00004752
#define TSG_PACKET_TYPE_MESSAGE_PACKET		0x00004750
#define TSG_PACKET_TYPE_AUTH			0x00004054
#define TSG_PACKET_TYPE_REAUTH			0x00005250
#define TSG_ASYNC_MESSAGE_CONSENT_MESSAGE	0x00000001
#define TSG_ASYNC_MESSAGE_SERVICE_MESSAGE	0x00000002
#define TSG_ASYNC_MESSAGE_REAUTH		0x00000003
#define TSG_TUNNEL_CALL_ASYNC_MSG_REQUEST	0x00000001
#define TSG_TUNNEL_CANCEL_ASYNC_MSG_REQUEST	0x00000002

#define TSG_NAP_CAPABILITY_QUAR_SOH		0x00000001
#define TSG_NAP_CAPABILITY_IDLE_TIMEOUT		0x00000002
#define TSG_MESSAGING_CAP_CONSENT_SIGN		0x00000004
#define TSG_MESSAGING_CAP_SERVICE_MSG		0x00000008
#define TSG_MESSAGING_CAP_REAUTH		0x00000010

typedef struct _TSG_PACKET_HEADER
{
	unsigned short ComponentId;
	unsigned short PacketId;
} TSG_PACKET_HEADER, *PTSG_PACKET_HEADER;

typedef struct _TSG_CAPABILITY_NAP
{
	unsigned long capabilities;
} TSG_CAPABILITY_NAP, *PTSG_CAPABILITY_NAP;

typedef union
{
	TSG_CAPABILITY_NAP tsgCapNap;
} TSG_CAPABILITIES_UNION, *PTSG_CAPABILITIES_UNION;

typedef struct _TSG_PACKET_CAPABILITIES
{
	unsigned long capabilityType;
	TSG_CAPABILITIES_UNION tsgPacket;
} TSG_PACKET_CAPABILITIES, *PTSG_PACKET_CAPABILITIES;

typedef struct _TSG_PACKET_VERSIONCAPS
{
	TSG_PACKET_HEADER tsgHeader;
	PTSG_PACKET_CAPABILITIES tsgCaps;
	unsigned long numCapabilities;
	unsigned short majorVersion;
	unsigned short minorVersion;
	unsigned short quarantineCapabilities;
} TSG_PACKET_VERSIONCAPS, *PTSG_PACKET_VERSIONCAPS;

typedef struct _TSG_PACKET_QUARCONFIGREQUEST
{
	unsigned long flags;
} TSG_PACKET_QUARCONFIGREQUEST, *PTSG_PACKET_QUARCONFIGREQUEST;

typedef struct _TSG_PACKET_QUARREQUEST
{
	unsigned long flags;
	wchar_t* machineName;
	unsigned long nameLength;
	byte* data;
	unsigned long dataLen;
} TSG_PACKET_QUARREQUEST, *PTSG_PACKET_QUARREQUEST;

typedef struct _TSG_REDIRECTION_FLAGS
{
	BOOL enableAllRedirections;
	BOOL disableAllRedirections;
	BOOL driveRedirectionDisabled;
	BOOL printerRedirectionDisabled;
	BOOL portRedirectionDisabled;
	BOOL reserved;
	BOOL clipboardRedirectionDisabled;
	BOOL pnpRedirectionDisabled;
} TSG_REDIRECTION_FLAGS, *PTSG_REDIRECTION_FLAGS;

typedef struct _TSG_PACKET_RESPONSE
{
	unsigned long flags;
	unsigned long reserved;
	byte* responseData;
	unsigned long responseDataLen;
	TSG_REDIRECTION_FLAGS redirectionFlags;
} TSG_PACKET_RESPONSE,	*PTSG_PACKET_RESPONSE;

typedef struct _TSG_PACKET_QUARENC_RESPONSE
{
	unsigned long flags;
	unsigned long certChainLen;
	wchar_t* certChainData;
	GUID nonce;
	PTSG_PACKET_VERSIONCAPS versionCaps;
} TSG_PACKET_QUARENC_RESPONSE, *PTSG_PACKET_QUARENC_RESPONSE;

typedef struct TSG_PACKET_STRING_MESSAGE
{
	long isDisplayMandatory;
	long isConsentMandatory;
	unsigned long msgBytes;
	wchar_t* msgBuffer;
} TSG_PACKET_STRING_MESSAGE, *PTSG_PACKET_STRING_MESSAGE;

typedef struct TSG_PACKET_REAUTH_MESSAGE
{
	unsigned __int64 tunnelContext;
} TSG_PACKET_REAUTH_MESSAGE, *PTSG_PACKET_REAUTH_MESSAGE;

typedef union
{
	PTSG_PACKET_STRING_MESSAGE consentMessage;
	PTSG_PACKET_STRING_MESSAGE serviceMessage;
	PTSG_PACKET_REAUTH_MESSAGE reauthMessage;
} TSG_PACKET_TYPE_MESSAGE_UNION, *PTSG_PACKET_TYPE_MESSAGE_UNION;

typedef struct _TSG_PACKET_MSG_RESPONSE
{
	unsigned long msgID;
	unsigned long msgType;
	long isMsgPresent;
	TSG_PACKET_TYPE_MESSAGE_UNION messagePacket;
} TSG_PACKET_MSG_RESPONSE, *PTSG_PACKET_MSG_RESPONSE;

typedef struct TSG_PACKET_CAPS_RESPONSE
{
	TSG_PACKET_QUARENC_RESPONSE pktQuarEncResponse;
	TSG_PACKET_MSG_RESPONSE pktConsentMessage;
} TSG_PACKET_CAPS_RESPONSE, *PTSG_PACKET_CAPS_RESPONSE;

typedef struct TSG_PACKET_MSG_REQUEST
{
	unsigned long maxMessagesPerBatch;
} TSG_PACKET_MSG_REQUEST, *PTSG_PACKET_MSG_REQUEST;

typedef struct _TSG_PACKET_AUTH
{
	TSG_PACKET_VERSIONCAPS tsgVersionCaps;
	unsigned long cookieLen;
	byte* cookie;
} TSG_PACKET_AUTH, *PTSG_PACKET_AUTH;

typedef union
{
	PTSG_PACKET_VERSIONCAPS packetVersionCaps;
	PTSG_PACKET_AUTH packetAuth;
} TSG_INITIAL_PACKET_TYPE_UNION, *PTSG_INITIAL_PACKET_TYPE_UNION;

typedef struct TSG_PACKET_REAUTH
{
	unsigned __int64 tunnelContext;
	unsigned long packetId;
	TSG_INITIAL_PACKET_TYPE_UNION tsgInitialPacket;
} TSG_PACKET_REAUTH, *PTSG_PACKET_REAUTH;

typedef union
{
	PTSG_PACKET_HEADER packetHeader;
	PTSG_PACKET_VERSIONCAPS packetVersionCaps;
	PTSG_PACKET_QUARCONFIGREQUEST packetQuarConfigRequest;
	PTSG_PACKET_QUARREQUEST packetQuarRequest;
	PTSG_PACKET_RESPONSE packetResponse;
	PTSG_PACKET_QUARENC_RESPONSE packetQuarEncResponse;
	PTSG_PACKET_CAPS_RESPONSE packetCapsResponse;
	PTSG_PACKET_MSG_REQUEST packetMsgRequest;
	PTSG_PACKET_MSG_RESPONSE packetMsgResponse;
	PTSG_PACKET_AUTH packetAuth;
	PTSG_PACKET_REAUTH packetReauth;
} TSG_PACKET_TYPE_UNION;

typedef struct _TSG_PACKET
{
	unsigned long packetId;
	TSG_PACKET_TYPE_UNION tsgPacket;
} TSG_PACKET, *PTSG_PACKET;

void Opnum0NotUsedOnWire(handle_t IDL_handle);

HRESULT TsProxyCreateTunnel(PTSG_PACKET tsgPacket, PTSG_PACKET* tsgPacketResponse,
		PTUNNEL_CONTEXT_HANDLE_SERIALIZE* tunnelContext, unsigned long* tunnelId);

HRESULT TsProxyAuthorizeTunnel(PTUNNEL_CONTEXT_HANDLE_NOSERIALIZE tunnelContext,
		PTSG_PACKET tsgPacket, PTSG_PACKET* tsgPacketResponse);

HRESULT TsProxyMakeTunnelCall(PTUNNEL_CONTEXT_HANDLE_NOSERIALIZE tunnelContext,
		unsigned long procId, PTSG_PACKET tsgPacket, PTSG_PACKET* tsgPacketResponse);

HRESULT TsProxyCreateChannel(PTUNNEL_CONTEXT_HANDLE_NOSERIALIZE tunnelContext,
		PTSENDPOINTINFO tsEndPointInfo, PCHANNEL_CONTEXT_HANDLE_SERIALIZE* channelContext, unsigned long* channelId);

void Opnum5NotUsedOnWire(handle_t IDL_handle);

HRESULT TsProxyCloseChannel(PCHANNEL_CONTEXT_HANDLE_NOSERIALIZE* context);

HRESULT TsProxyCloseTunnel(PTUNNEL_CONTEXT_HANDLE_SERIALIZE* context);

DWORD TsProxySetupReceivePipe(handle_t IDL_handle, byte pRpcMessage[]);

DWORD TsProxySendToServer(handle_t IDL_handle, byte pRpcMessage[], uint32 count, uint32* lengths);

boolean tsg_connect(rdpTsg* tsg, const char* hostname, uint16 port);

int tsg_write(rdpTsg* tsg, uint8* data, uint32 length);
int tsg_read(rdpTsg* tsg, uint8* data, uint32 length);

rdpTsg* tsg_new(rdpTransport* transport);
void tsg_free(rdpTsg* tsg);

#ifdef WITH_DEBUG_TSG
#define DEBUG_TSG(fmt, ...) DEBUG_CLASS(TSG, fmt, ## __VA_ARGS__)
#else
#define DEBUG_TSG(fmt, ...) DEBUG_NULL(fmt, ## __VA_ARGS__)
#endif

#endif /* FREERDP_CORE_TSG_H */
