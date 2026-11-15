/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Internal HTTP RD Gateway state shared with the protocol tests.
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

#ifndef FREERDP_LIB_CORE_GATEWAY_RDG_PRIVATE_H
#define FREERDP_LIB_CORE_GATEWAY_RDG_PRIVATE_H

#include <freerdp/utils/smartcardlogon.h>
#include "rdg.h"
#include "http.h"
#include "websocket.h"
#include "../credssp_auth.h"

typedef struct
{
	TRANSFER_ENCODING httpTransferEncoding;
	BOOL isWebsocketTransport;
	http_encoding_chunked_context chunked;
	websocket_context* websocket;
} rdg_http_encoding_context;

struct rdp_rdg
{
	rdpContext* context;
	BOOL attached;
	BOOL channelClosed;
	BOOL closeResponsePending;
	UINT64 redirectDeadline;
	char* redirectGatewayHost;
	UINT32 redirectGatewayPort;
	BIO* frontBio;
	rdpTls* tlsIn;
	rdpTls* tlsOut;
	rdpCredsspAuth* auth;
	HttpContext* http;
	CRITICAL_SECTION writeSection;

	int state;
	UINT16 packetRemainingCount;
	UINT16 reserved1;
	int timeout;
	UINT16 extAuth;
	UINT16 reserved2;
	rdg_http_encoding_context transferEncoding;

	SmartcardCertInfo* smartcard;
	wLog* log;

	/* The header of the next packet, as much of it as has arrived: a read that would block returns
	 * without losing it, and the next read goes on from there (rdg_read_data_packet). */
	BYTE nextHeader[8];
	size_t nextHeaderLengthRead;
};

enum
{
	RDG_CLIENT_STATE_INITIAL,
	RDG_CLIENT_STATE_HANDSHAKE,
	RDG_CLIENT_STATE_TUNNEL_CREATE,
	RDG_CLIENT_STATE_TUNNEL_AUTHORIZE,
	RDG_CLIENT_STATE_CHANNEL_CREATE,
	RDG_CLIENT_STATE_OPENED,
};

#endif /* FREERDP_LIB_CORE_GATEWAY_RDG_PRIVATE_H */
