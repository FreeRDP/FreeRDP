/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Terminal Server Gateway (TSG)
 *
 * Copyright 2012 Fujitsu Technology Solutions GmbH
 * Copyright 2012 Dmitrij Jasnov <dmitrij.jasnov@ts.fujitsu.com>
 * Copyright 2012 Marc-Andre Moreau <marcandre.moreau@gmail.com>
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

#include "../transport.h"

#include <winpr/rpc.h>
#include <winpr/winpr.h>
#include <winpr/wtypes.h>
#include <winpr/synch.h>
#include <winpr/error.h>

#include <time.h>
#include <freerdp/types.h>
#include <freerdp/settings.h>

#include <freerdp/log.h>

enum _TSG_STATE
{
	TSG_STATE_INITIAL,
	TSG_STATE_CONNECTED,
	TSG_STATE_AUTHORIZED,
	TSG_STATE_CHANNEL_CREATED,
	TSG_STATE_PIPE_CREATED,
	TSG_STATE_TUNNEL_CLOSE_PENDING,
	TSG_STATE_CHANNEL_CLOSE_PENDING,
	TSG_STATE_FINAL
};
typedef enum _TSG_STATE TSG_STATE;

struct rdp_tsg
{
	BIO* bio;
	rdpRpc* rpc;
	UINT16 Port;
	LPWSTR Hostname;
	LPWSTR MachineName;
	TSG_STATE state;
	rdpSettings* settings;
	rdpTransport* transport;
	CONTEXT_HANDLE TunnelContext;
	CONTEXT_HANDLE ChannelContext;
};

typedef WCHAR* RESOURCENAME;

#define TsProxyCreateTunnelOpnum		1
#define TsProxyAuthorizeTunnelOpnum		2
#define TsProxyMakeTunnelCallOpnum		3
#define TsProxyCreateChannelOpnum		4
#define TsProxyUnused5Opnum			5
#define TsProxyCloseChannelOpnum		6
#define TsProxyCloseTunnelOpnum			7
#define TsProxySetupReceivePipeOpnum		8
#define TsProxySendToServerOpnum		9

#define MAX_RESOURCE_NAMES			50

typedef struct _tsendpointinfo
{
	RESOURCENAME* resourceName;
	UINT32 numResourceNames;
	RESOURCENAME* alternateResourceNames;
	UINT16 numAlternateResourceNames;
	UINT32 Port;
} TSENDPOINTINFO, *PTSENDPOINTINFO;

#define TS_GATEWAY_TRANSPORT				0x5452

#define TSG_PACKET_TYPE_HEADER				0x00004844
#define TSG_PACKET_TYPE_VERSIONCAPS			0x00005643
#define TSG_PACKET_TYPE_QUARCONFIGREQUEST		0x00005143
#define TSG_PACKET_TYPE_QUARREQUEST			0x00005152
#define TSG_PACKET_TYPE_RESPONSE			0x00005052
#define TSG_PACKET_TYPE_QUARENC_RESPONSE		0x00004552
#define TSG_CAPABILITY_TYPE_NAP				0x00000001
#define TSG_PACKET_TYPE_CAPS_RESPONSE			0x00004350
#define TSG_PACKET_TYPE_MSGREQUEST_PACKET		0x00004752
#define TSG_PACKET_TYPE_MESSAGE_PACKET			0x00004750
#define TSG_PACKET_TYPE_AUTH				0x00004054
#define TSG_PACKET_TYPE_REAUTH				0x00005250

#define TSG_ASYNC_MESSAGE_CONSENT_MESSAGE		0x00000001
#define TSG_ASYNC_MESSAGE_SERVICE_MESSAGE		0x00000002
#define TSG_ASYNC_MESSAGE_REAUTH			0x00000003

#define TSG_TUNNEL_CALL_ASYNC_MSG_REQUEST		0x00000001
#define TSG_TUNNEL_CANCEL_ASYNC_MSG_REQUEST		0x00000002

#define TSG_NAP_CAPABILITY_QUAR_SOH			0x00000001
#define TSG_NAP_CAPABILITY_IDLE_TIMEOUT			0x00000002
#define TSG_MESSAGING_CAP_CONSENT_SIGN			0x00000004
#define TSG_MESSAGING_CAP_SERVICE_MSG			0x00000008
#define TSG_MESSAGING_CAP_REAUTH			0x00000010
#define TSG_MESSAGING_MAX_MESSAGE_LENGTH		65536

/* Error Codes */

#define E_PROXY_INTERNALERROR				0x800759D8
#define E_PROXY_RAP_ACCESSDENIED			0x800759DA
#define E_PROXY_NAP_ACCESSDENIED			0x800759DB
#define E_PROXY_TS_CONNECTFAILED			0x800759DD
#define E_PROXY_ALREADYDISCONNECTED			0x800759DF
#define E_PROXY_QUARANTINE_ACCESSDENIED			0x800759ED
#define E_PROXY_NOCERTAVAILABLE				0x800759EE
#define E_PROXY_COOKIE_BADPACKET			0x800759F7
#define E_PROXY_COOKIE_AUTHENTICATION_ACCESS_DENIED	0x800759F8
#define E_PROXY_UNSUPPORTED_AUTHENTICATION_METHOD	0x800759F9
#define E_PROXY_CAPABILITYMISMATCH			0x800759E9

#define E_PROXY_NOTSUPPORTED				0x000059E8
#define E_PROXY_MAXCONNECTIONSREACHED			0x000059E6
#define E_PROXY_SESSIONTIMEOUT				0x000059F6
#define E_PROXY_REAUTH_AUTHN_FAILED			0X000059FA
#define E_PROXY_REAUTH_CAP_FAILED			0x000059FB
#define E_PROXY_REAUTH_RAP_FAILED			0x000059FC
#define E_PROXY_SDR_NOT_SUPPORTED_BY_TS			0x000059FD
#define E_PROXY_REAUTH_NAP_FAILED			0x00005A00
#define E_PROXY_CONNECTIONABORTED			0x000004D4

typedef struct _TSG_PACKET_HEADER
{
	UINT16 ComponentId;
	UINT16 PacketId;
} TSG_PACKET_HEADER, *PTSG_PACKET_HEADER;

typedef struct _TSG_CAPABILITY_NAP
{
	UINT32 capabilities;
} TSG_CAPABILITY_NAP, *PTSG_CAPABILITY_NAP;

typedef union
{
	TSG_CAPABILITY_NAP tsgCapNap;
} TSG_CAPABILITIES_UNION, *PTSG_CAPABILITIES_UNION;

typedef struct _TSG_PACKET_CAPABILITIES
{
	UINT32 capabilityType;
	TSG_CAPABILITIES_UNION tsgPacket;
} TSG_PACKET_CAPABILITIES, *PTSG_PACKET_CAPABILITIES;

typedef struct _TSG_PACKET_VERSIONCAPS
{
	TSG_PACKET_HEADER tsgHeader;
	PTSG_PACKET_CAPABILITIES tsgCaps;
	UINT32 numCapabilities;
	UINT16 majorVersion;
	UINT16 minorVersion;
	UINT16 quarantineCapabilities;
} TSG_PACKET_VERSIONCAPS, *PTSG_PACKET_VERSIONCAPS;

typedef struct _TSG_PACKET_QUARCONFIGREQUEST
{
	UINT32 flags;
} TSG_PACKET_QUARCONFIGREQUEST, *PTSG_PACKET_QUARCONFIGREQUEST;

typedef struct _TSG_PACKET_QUARREQUEST
{
	UINT32 flags;
	WCHAR* machineName;
	UINT32 nameLength;
	BYTE* data;
	UINT32 dataLen;
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
	UINT32 flags;
	UINT32 reserved;
	BYTE* responseData;
	UINT32 responseDataLen;
	TSG_REDIRECTION_FLAGS redirectionFlags;
} TSG_PACKET_RESPONSE,	*PTSG_PACKET_RESPONSE;

typedef struct _TSG_PACKET_QUARENC_RESPONSE
{
	UINT32 flags;
	UINT32 certChainLen;
	WCHAR* certChainData;
	GUID nonce;
	PTSG_PACKET_VERSIONCAPS versionCaps;
} TSG_PACKET_QUARENC_RESPONSE, *PTSG_PACKET_QUARENC_RESPONSE;

typedef struct TSG_PACKET_STRING_MESSAGE
{
	INT32 isDisplayMandatory;
	INT32 isConsentMandatory;
	UINT32 msgBytes;
	WCHAR* msgBuffer;
} TSG_PACKET_STRING_MESSAGE, *PTSG_PACKET_STRING_MESSAGE;

typedef struct TSG_PACKET_REAUTH_MESSAGE
{
	UINT64 tunnelContext;
} TSG_PACKET_REAUTH_MESSAGE, *PTSG_PACKET_REAUTH_MESSAGE;

typedef union
{
	PTSG_PACKET_STRING_MESSAGE consentMessage;
	PTSG_PACKET_STRING_MESSAGE serviceMessage;
	PTSG_PACKET_REAUTH_MESSAGE reauthMessage;
} TSG_PACKET_TYPE_MESSAGE_UNION, *PTSG_PACKET_TYPE_MESSAGE_UNION;

typedef struct _TSG_PACKET_MSG_RESPONSE
{
	UINT32 msgID;
	UINT32 msgType;
	INT32 isMsgPresent;
	TSG_PACKET_TYPE_MESSAGE_UNION messagePacket;
} TSG_PACKET_MSG_RESPONSE, *PTSG_PACKET_MSG_RESPONSE;

typedef struct TSG_PACKET_CAPS_RESPONSE
{
	TSG_PACKET_QUARENC_RESPONSE pktQuarEncResponse;
	TSG_PACKET_MSG_RESPONSE pktConsentMessage;
} TSG_PACKET_CAPS_RESPONSE, *PTSG_PACKET_CAPS_RESPONSE;

typedef struct TSG_PACKET_MSG_REQUEST
{
	UINT32 maxMessagesPerBatch;
} TSG_PACKET_MSG_REQUEST, *PTSG_PACKET_MSG_REQUEST;

typedef struct _TSG_PACKET_AUTH
{
	TSG_PACKET_VERSIONCAPS tsgVersionCaps;
	UINT32 cookieLen;
	BYTE* cookie;
} TSG_PACKET_AUTH, *PTSG_PACKET_AUTH;

typedef union
{
	PTSG_PACKET_VERSIONCAPS packetVersionCaps;
	PTSG_PACKET_AUTH packetAuth;
} TSG_INITIAL_PACKET_TYPE_UNION, *PTSG_INITIAL_PACKET_TYPE_UNION;

typedef struct TSG_PACKET_REAUTH
{
	UINT64 tunnelContext;
	UINT32 packetId;
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
	UINT32 packetId;
	TSG_PACKET_TYPE_UNION tsgPacket;
} TSG_PACKET, *PTSG_PACKET;

BOOL TsProxyCreateTunnel(rdpTsg* tsg, PTSG_PACKET tsgPacket, PTSG_PACKET* tsgPacketResponse,
			PTUNNEL_CONTEXT_HANDLE_SERIALIZE* tunnelContext, UINT32* tunnelId);

DWORD TsProxySendToServer(handle_t IDL_handle, BYTE pRpcMessage[], UINT32 count, UINT32* lengths);

int tsg_transition_to_state(rdpTsg* tsg, TSG_STATE state);

BOOL tsg_connect(rdpTsg* tsg, const char* hostname, UINT16 port, int timeout);
BOOL tsg_disconnect(rdpTsg* tsg);

int tsg_write(rdpTsg* tsg, BYTE* data, UINT32 length);
int tsg_read(rdpTsg* tsg, BYTE* data, UINT32 length);

int tsg_recv_pdu(rdpTsg* tsg, RPC_PDU* pdu);
int tsg_check(rdpTsg* tsg);

rdpTsg* tsg_new(rdpTransport* transport);
void tsg_free(rdpTsg* tsg);

BIO_METHOD* BIO_s_tsg(void);

#endif /* FREERDP_CORE_TSG_H */
