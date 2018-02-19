/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Remote Desktop Gateway (RDG)
 *
 * Copyright 2015 Denis Vincent <dvincent@devolutions.net>
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

#ifndef FREERDP_LIB_CORE_GATEWAY_RDG_H
#define FREERDP_LIB_CORE_GATEWAY_RDG_H


#include <winpr/wtypes.h>
#include <winpr/stream.h>
#include <winpr/collections.h>
#include <winpr/interlocked.h>

#include <freerdp/log.h>
#include <freerdp/utils/ringbuffer.h>
#include <freerdp/api.h>

#include <freerdp/freerdp.h>
#include <freerdp/crypto/tls.h>
#include <freerdp/types.h>
#include <freerdp/settings.h>

typedef struct rdp_rdg rdpRdg;

#include "http.h"
#include "ntlm.h"
#include "../transport.h"

/* HTTP channel response fields present flags. */
#define HTTP_CHANNEL_RESPONSE_FIELD_CHANNELID 0x1
#define HTTP_CHANNEL_RESPONSE_OPTIONAL 0x2
#define HTTP_CHANNEL_RESPONSE_FIELD_UDPPORT 0x4

/* HTTP extended auth. */
#define HTTP_EXTENDED_AUTH_NONE 0x0
#define HTTP_EXTENDED_AUTH_SC 0x1   /* Smart card authentication. */
#define HTTP_EXTENDED_AUTH_PAA 0x02   /* Pluggable authentication. */
#define HTTP_EXTENDED_AUTH_SSPI_NTLM 0x04   /* NTLM extended authentication. */

/* HTTP packet types. */
#define PKT_TYPE_HANDSHAKE_REQUEST 0x1
#define PKT_TYPE_HANDSHAKE_RESPONSE 0x2
#define PKT_TYPE_EXTENDED_AUTH_MSG 0x3
#define PKT_TYPE_TUNNEL_CREATE 0x4
#define PKT_TYPE_TUNNEL_RESPONSE 0x5
#define PKT_TYPE_TUNNEL_AUTH 0x6
#define PKT_TYPE_TUNNEL_AUTH_RESPONSE 0x7
#define PKT_TYPE_CHANNEL_CREATE 0x8
#define PKT_TYPE_CHANNEL_RESPONSE 0x9
#define PKT_TYPE_DATA 0xA
#define PKT_TYPE_SERVICE_MESSAGE 0xB
#define PKT_TYPE_REAUTH_MESSAGE 0xC
#define PKT_TYPE_KEEPALIVE 0xD
#define PKT_TYPE_CLOSE_CHANNEL 0x10
#define PKT_TYPE_CLOSE_CHANNEL_RESPONSE 0x11

/* HTTP tunnel auth fields present flags. */
#define HTTP_TUNNEL_AUTH_FIELD_SOH 0x1

/* HTTP tunnel auth response fields present flags. */
#define HTTP_TUNNEL_AUTH_RESPONSE_FIELD_REDIR_FLAGS 0x1
#define HTTP_TUNNEL_AUTH_RESPONSE_FIELD_IDLE_TIMEOUT 0x2
#define HTTP_TUNNEL_AUTH_RESPONSE_FIELD_SOH_RESPONSE 0x4

/* HTTP tunnel packet fields present flags. */
#define HTTP_TUNNEL_PACKET_FIELD_PAA_COOKIE 0x1
#define HTTP_TUNNEL_PACKET_FIELD_REAUTH 0x2

/* HTTP tunnel redir flags. */
#define HTTP_TUNNEL_REDIR_ENABLE_ALL 0x80000000
#define HTTP_TUNNEL_REDIR_DISABLE_ALL 0x40000000
#define HTTP_TUNNEL_REDIR_DISABLE_DRIVE 0x1
#define HTTP_TUNNEL_REDIR_DISABLE_PRINTER 0x2
#define HTTP_TUNNEL_REDIR_DISABLE_PORT 0x4
#define HTTP_TUNNEL_REDIR_DISABLE_CLIPBOARD 0x8
#define HTTP_TUNNEL_REDIR_DISABLE_PNP 0x10

/* HTTP tunnel response fields present flags. */
#define HTTP_TUNNEL_RESPONSE_FIELD_TUNNEL_ID 0x1
#define HTTP_TUNNEL_RESPONSE_FIELD_CAPS 0x2
#define HTTP_TUNNEL_RESPONSE_FIELD_SOH_REQ 0x4
#define HTTP_TUNNEL_RESPONSE_FIELD_CONSENT_MSG 0x10

/* HTTP capability type enumeration. */
#define HTTP_CAPABILITY_TYPE_QUAR_SOH 0x1
#define HTTP_CAPABILITY_IDLE_TIMEOUT 0x2
#define HTTP_CAPABILITY_MESSAGING_CONSENT_SIGN 0x4
#define HTTP_CAPABILITY_MESSAGING_SERVICE_MSG 0x8
#define HTTP_CAPABILITY_REAUTH 0x10
#define HTTP_CAPABILITY_UDP_TRANSPORT 0x20


enum
{
	RDG_CLIENT_STATE_INITIAL,
	RDG_CLIENT_STATE_OUT_CHANNEL_REQUEST,
	RDG_CLIENT_STATE_OUT_CHANNEL_AUTHORIZE,
	RDG_CLIENT_STATE_OUT_CHANNEL_AUTHORIZED,
	RDG_CLIENT_STATE_IN_CHANNEL_REQUEST,
	RDG_CLIENT_STATE_IN_CHANNEL_AUTHORIZE,
	RDG_CLIENT_STATE_IN_CHANNEL_AUTHORIZED,
	RDG_CLIENT_STATE_HANDSHAKE,
	RDG_CLIENT_STATE_TUNNEL_CREATE,
	RDG_CLIENT_STATE_TUNNEL_AUTHORIZE,
	RDG_CLIENT_STATE_CHANNEL_CREATE,
	RDG_CLIENT_STATE_OPENED,
	RDG_CLIENT_STATE_CLOSE,
	RDG_CLIENT_STATE_CLOSED,
	RDG_CLIENT_STATE_NOT_FOUND,
};

struct rdp_rdg
{
	rdpContext* context;
	rdpSettings* settings;
	BIO* frontBio;
	rdpTls* tlsIn;
	rdpTls* tlsOut;
	rdpNtlm* ntlm;
	HttpContext* http;
	HANDLE readEvent;
	CRITICAL_SECTION writeSection;

	UUID guid;

	int state;
	UINT16 packetRemainingCount;
	int timeout;
	UINT16 extAuth;
};


FREERDP_LOCAL rdpRdg* rdg_new(rdpTransport* transport);
FREERDP_LOCAL void rdg_free(rdpRdg* rdg);

FREERDP_LOCAL BOOL rdg_connect(rdpRdg* rdg, const char* hostname, UINT16 port,
                               int timeout);
FREERDP_LOCAL DWORD rdg_get_event_handles(rdpRdg* rdg, HANDLE* events,
        DWORD count);


#endif /* FREERDP_LIB_CORE_GATEWAY_RDG_H */
